#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

/* -- IORead ---------------------------------------------------------- */

typedef struct {
    char *buffer;
    size_t buflen;
    int errno_save;
} IORead;

static
void IORead_init(IORead *ioread)
{
    *ioread = (IORead) {};
}

static
void IORead_free(IORead *ioread)
{
    free(ioread->buffer);
}

static
const char *IORead_next(IORead *ioread)
{
    enum {
        delm = 0,
    };

    ssize_t nread;

    errno = 0;
    nread = getdelim(&ioread->buffer, &ioread->buflen, delm, stdin);

    if (nread == -1) {
        ioread->errno_save = errno;
        if (errno)
            warn("getdelim");
        return NULL;
    }

    ioread->buffer[nread - 1] = '\0';
    return ioread->buffer;
}

/* -- misc ------------------------------------------------------------ */

const char * file_hash(const char *filename, char *buffer, size_t buflen)
{
    enum {
        r = 0,
        w = 1,
        min_buflen = 33,
    };

    pid_t pid = -1;
    ssize_t nread;
    int pipefd[2] = {-1, -1};

    if (buflen < min_buflen) {
        warnx("min buflen is %d - %zu is not enough", min_buflen, buflen);
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
            if (dup2(pipefd[r], STDIN_FILENO) == -1)
                err(1, "dup2(%d, %d)", pipefd[r], STDIN_FILENO);
            if (close(pipefd[r]) == -1)
                err(1, "close(%d)", pipefd[r]);
            if (dup2(pipefd[w], STDOUT_FILENO) == -1)
                err(1, "dup2(%d, %d)", pipefd[w], STDOUT_FILENO);
            if (close(pipefd[w]) == -1)
                err(1, "close(%d)", pipefd[w]);
            execlp("md5sum", "md5sum", filename, NULL);
            err(1, "execlp");

        default:
            break;
    }

    if (close(pipefd[w]) == -1) {
        warnx("close(%d)", pipefd[w]);
        goto fail;
    }
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

    if (close(pipefd[r]) == -1)
        warn("close(%d)", pipefd[r]);

    buffer[nread - 1] = '\0';
    return buffer;
    
fail:
    if (pid > 0 && waitpid(pid, NULL, 0) == -1)
        warn("waitpid(%d, NULL, 0)", pid);

    if (pipefd[r] != -1 && close(pipefd[r]) == -1)
        warn("close(%d)", pipefd[r]);

    if (pipefd[w] != -1 && close(pipefd[w]) == -1)
        warn("close(%d)", pipefd[w]);

    return NULL;
}


/* -- main ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    IORead ioread;
    int ex = EXIT_FAILURE;

    const char *fname;

    IORead_init(&ioread);

    while (fname = IORead_next(&ioread), fname != NULL) {
        char buffer[33];
        const char *hash;

        hash = file_hash(fname, buffer, sizeof(buffer));
        if (!hash) {
            warnx("skipping %s: could not compute hash", fname);
            continue;  // TODO - count failure?
        }

    }

    if (ioread.errno_save == 0)
        ex = EXIT_SUCCESS;

    IORead_free(&ioread);
    return ex;
}
