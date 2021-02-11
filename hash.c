#include <ctype.h>
#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"
#include "hash.h"

struct Hash {
    const char *hashprg;
    char *buffer;
};

enum {
    Hash_checksum_length = 128, // ok for anything up to sha512
    Hash_buflen = Hash_checksum_length + 1,
};

void Hash_del(Hash *hash)
{
    if (!hash)
        return;

    free((void *)hash->hashprg);
    free((void *)hash->buffer);
    free(hash);
}

Hash *Hash_new(const char *hashprg)
{
    Hash *hash = malloc(sizeof(Hash));
    if (!hash) {
        warn("malloc");
        goto fail;
    }
    *hash = (Hash){};

    hash->hashprg = strdup(hashprg);
    if (!hash->hashprg) {
        warn("strdup");
        goto fail;
    }

    hash->buffer = malloc(Hash_buflen);
    if (!hash->buffer) {
        warn("malloc");
        goto fail;
    }

    return hash;

fail:
    Hash_del(hash);
    return NULL;
}

static
int Hash_wait(pid_t child)
{
    for (;;) {
        int w, status;

        w = waitpid(child, &status, WUNTRACED | WCONTINUED);
        if (w == -1) {
            warn("waitpid");
            return -1;
        }

        if (WIFEXITED(status)) {
            return WEXITSTATUS(status);
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
void Hash_exec(const char * const hashprg, int r, int w, const char *filename)
{
    if (dup2(r, STDIN_FILENO) == -1)
        err(1, "dup2(%d, %d)", r, STDIN_FILENO);
    if (Util_fdclose(&r))
        exit(EXIT_FAILURE);
    if (dup2(w, STDOUT_FILENO) == -1)
        err(1, "dup2(%d, %d)", w, STDOUT_FILENO);
    if (Util_fdclose(&w) == -1)
        exit(EXIT_FAILURE);
    execlp(hashprg, hashprg, filename, NULL);
    err(1, "execlp");
}

static
int Hash_read(const Hash *hash, int r)
{
    ssize_t n;
    char *buffer;
    int room;

    room = Hash_buflen;
    buffer = hash->buffer;

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

    warnx("invalid hash: %.*s", Hash_buflen, hash->buffer);
    return -1;
}

const char * Hash_file(const Hash *hash, const char *filename)
{
    enum {
        r = 0,
        w = 1,
    };

    pid_t pid = -1;
    int pipefd[2] = {-1, -1};

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
            Hash_exec(hash->hashprg, pipefd[r], pipefd[w], filename);

        default:
            break;
    }

    if (Util_fdclose(&pipefd[w]) == -1)
        goto fail;
    pipefd[w] = -1;

    if (Hash_read(hash, pipefd[r]) == -1)
        goto fail;

    if (Hash_wait(pid))
        goto fail;

    Util_fdclose(&pipefd[r]);

    return hash->buffer;
    
fail:
    if (pid > 0)
        Hash_wait(pid);

    Util_fdclose(&pipefd[r]);
    Util_fdclose(&pipefd[w]);
    return NULL;
}
