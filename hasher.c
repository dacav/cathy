#include <ctype.h>
#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"
#include "hasher.h"

struct Hasher {
    const char *hashprg;
    const char *compprg;
    char *buffer;
};

enum {
    Hasher_checksum_length = 128, // ok for anything up to sha512
    Hasher_buflen = Hasher_checksum_length + 1,
};

void Hasher_del(Hasher *hasher)
{
    if (!hasher)
        return;

    free((void *)hasher->hashprg);
    free((void *)hasher->compprg);
    free((void *)hasher->buffer);
    free(hasher);
}

Hasher *Hasher_new(const char *hashprg, const char *compprg)
{
    Hasher *hasher = malloc(sizeof(Hasher));
    if (!hasher) {
        warn("malloc");
        goto fail;
    }
    *hasher = (Hasher){};

    hasher->hashprg = strdup(hashprg);
    if (!hasher->hashprg) {
        warn("strdup");
        goto fail;
    }

    hasher->compprg = strdup(compprg);
    if (!hasher->compprg) {
        warn("strdup");
        goto fail;
    }

    hasher->buffer = malloc(Hasher_buflen);
    if (!hasher->buffer) {
        warn("malloc");
        goto fail;
    }

    return hasher;

fail:
    Hasher_del(hasher);
    return NULL;
}

static
int Hasher_wait(pid_t child, int *exit_status)
{
    for (;;) {
        int w, status;

        w = waitpid(child, &status, WUNTRACED | WCONTINUED);
        if (w == -1) {
            warn("waitpid");
            return -1;
        }

        if (WIFEXITED(status)) {
            if (exit_status)
                *exit_status = WEXITSTATUS(status);
            return 0;
        }

        if (WIFSIGNALED(status)) {
            warnx("subprocess killed by signal %d", WTERMSIG(status));
            return -1;
        }

        if (WIFSTOPPED(status))
            warnx("subprocess stopped by signal %d", WSTOPSIG(status));
        else if (WIFCONTINUED(status))
            warnx("subprocess continued");
    }
}

static
void Hasher_exec(const char * const hashprg, int r, int w, const char *path)
{
    if (dup2(r, STDIN_FILENO) == -1)
        err(1, "dup2(%d, %d)", r, STDIN_FILENO);
    if (Util_fdclose(&r))
        exit(EXIT_FAILURE);
    if (dup2(w, STDOUT_FILENO) == -1)
        err(1, "dup2(%d, %d)", w, STDOUT_FILENO);
    if (Util_fdclose(&w) == -1)
        exit(EXIT_FAILURE);
    execlp(hashprg, hashprg, path, NULL);
    err(1, "execlp %s %s", hashprg, path);
}

static
int Hasher_read(const Hasher *hasher, int r)
{
    ssize_t n;
    char *buffer;
    int room;

    room = Hasher_buflen;
    buffer = hasher->buffer;

    do {
        n = read(r, buffer, room);
        if (n == -1) {
            warn("read(%d, ...)", r);
            return -1;
        }

        for (int i = 0; i < n; ++i)
            if (isspace(buffer[i])) {
                buffer[i] = '\0';
                return 0;
            }
            else if (buffer[i] == '\0')
                return 0;

        room -= n;
        buffer += n;
    } while (room && n);

    if (n == 0)
        warnx("invalid hash, zero bytes answer from subcommand");
    else
        warnx("invalid hash: %.*s", Hasher_buflen, hasher->buffer);
    return -1;
}

int Hasher_comp_files(const Hasher *hash,
                      const char *path1,
                      const char *path2,
                      bool *equals)
{
    pid_t pid;
    int exit_status;

    pid = fork();
    switch (pid) {
    case -1:
        warn("fork");
        return -1;

    case 0:
        close(STDIN_FILENO);
        execlp(hash->compprg, hash->compprg, path1, path2, NULL);
        err(1, "execlp %s %s %s", hash->compprg, path1, path2);

    default:
        if (Hasher_wait(pid, &exit_status))
            return -1;
        *equals = !exit_status;
        return 0;
    }
}

const char * Hasher_hash_file(const Hasher *hasher, const char *path)
{
    enum {
        r = 0,
        w = 1,
    };

    pid_t pid = -1;
    int pipefd[2] = {-1, -1};
    int exit_status;

    if (pipe(pipefd) == -1) {
        warn("pipe");
        goto fail;
    }

    pid = fork();
    switch (pid) {
    case -1:
        warn("fork");
        goto fail;

    case 0:
        Hasher_exec(hasher->hashprg, pipefd[r], pipefd[w], path);

    default:
        break;
    }

    if (Util_fdclose(&pipefd[w]) == -1)
        goto fail;
    pipefd[w] = -1;

    if (Hasher_read(hasher, pipefd[r]) == -1)
        goto fail;

    if (Hasher_wait(pid, &exit_status) || exit_status)
        goto fail;

    Util_fdclose(&pipefd[r]);

    return hasher->buffer;
    
fail:
    if (pid > 0)
        Hasher_wait(pid, NULL);

    Util_fdclose(&pipefd[r]);
    Util_fdclose(&pipefd[w]);
    return NULL;
}
