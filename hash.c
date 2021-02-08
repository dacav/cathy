#include <err.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "util.h"

enum {
    Hash_checksum_length = 32,
    Hash_buflen = Hash_checksum_length + 1,
};

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
void Hash_exec(int r, int w, const char *filename)
{
    if (dup2(r, STDIN_FILENO) == -1)
        err(1, "dup2(%d, %d)", r, STDIN_FILENO);
    if (Util_fdclose(&r))
        exit(EXIT_FAILURE);
    if (dup2(w, STDOUT_FILENO) == -1)
        err(1, "dup2(%d, %d)", w, STDOUT_FILENO);
    if (Util_fdclose(&w) == -1)
        exit(EXIT_FAILURE);
    execlp("md5sum", "md5sum", filename, NULL);
    err(1, "execlp");
}

const char * Hash_file(const char *filename, char *buffer, size_t buflen)
{
    enum {
        r = 0,
        w = 1,
    };

    pid_t pid = -1;
    ssize_t nread;
    int pipefd[2] = {-1, -1};

    if (buflen < Hash_buflen) {
        warnx("min buflen is %d - %zu is not enough", Hash_buflen, buflen);
        goto fail;
    }

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
            Hash_exec(pipefd[r], pipefd[w], filename);

        default:
            break;
    }

    if (Util_fdclose(&pipefd[w]) == -1)
        goto fail;
    pipefd[w] = -1;

    nread = read(pipefd[r], buffer, buflen);
    if (nread == -1) {
        warn("read(%d, ...)", pipefd[r]);
        goto fail;
    }

    if (nread != buflen) {
        if (nread > 0)
            warnx("read(%d) -> 0", pipefd[r]);
        goto fail;
    }

    if (Hash_wait(pid))
        goto fail;

    Util_fdclose(&pipefd[r]);

    buffer[nread - 1] = '\0';
    return buffer;
    
fail:
    if (pid > 0)
        Hash_wait(pid);

    Util_fdclose(&pipefd[r]);
    Util_fdclose(&pipefd[w]);
    return NULL;
}
