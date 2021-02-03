#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <limits.h>

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

/* -- OutDir ---------------------------------------------------------- */

enum {
    OutDir_PREFIX = 3,
};

typedef struct {
    char *buffer;
    size_t base_path_len;
    size_t hashlen;
} OutDir;

static
void OutDir_free(OutDir *outdir)
{
    free(outdir->buffer);
}

static
int OutDir_init(OutDir *outdir, const char *path, size_t hashlen)
{
    char *rpath;

    *outdir = (OutDir){
        .hashlen = hashlen,
    };

    rpath = realpath(path, NULL);
    if (!rpath) {
        warn("realpath(%s, NULL)", path);
        goto fail;
    }

    outdir->base_path_len = strlen(rpath);

    /* outdir + '/' + hash[0:N] + '/' + hash[N:] + '\0' */
    size_t len = outdir->base_path_len + 3 + hashlen;

    outdir->buffer = realloc(rpath, len);
    if (!outdir->buffer)
        goto fail;
    rpath = NULL;

    outdir->buffer[outdir->base_path_len++] = '/';
    outdir->buffer[outdir->base_path_len] = '\0';

    return 0;

fail:
    free(rpath);
    OutDir_free(outdir);
    return -1;
}

static
const char * OutDir_path(OutDir *outdir, const char *hash)
{
    const size_t hashlen = outdir->hashlen;
    char *buffer = outdir->buffer;
    int w = outdir->base_path_len;

    for (int i = 0; i < hashlen && hash[i]; ++i) {
        if (i == OutDir_PREFIX)
            buffer[w++] = '/';
        buffer[w++] = hash[i];
    }

    buffer[w++] = '\0';

    return outdir->buffer;
}

/* -- Hash ------------------------------------------------------------ */

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
    if (close(r) == -1)
        err(1, "close(%d)", r);
    if (dup2(w, STDOUT_FILENO) == -1)
        err(1, "dup2(%d, %d)", w, STDOUT_FILENO);
    if (close(w) == -1)
        err(1, "close(%d)", w);
    execlp("md5sum", "md5sum", filename, NULL);
    err(1, "execlp");
}

static
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

    if (Hash_wait(pid))
        goto fail;

    if (close(pipefd[r]) == -1)
        warn("close(%d)", pipefd[r]);

    buffer[nread - 1] = '\0';
    return buffer;
    
fail:
    if (pid > 0)
        Hash_wait(pid);

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
    OutDir outdir;
    int fails = 0;

    const char *fname;

    const char *outdir_path = "./cathy"; // TODO: argv

    IORead_init(&ioread);

    if (OutDir_init(&outdir, outdir_path, Hash_checksum_length))
        goto exit;

    while (fname = IORead_next(&ioread), fname != NULL) {
        char buffer[Hash_buflen];
        const char *hash;

        hash = Hash_file(fname, buffer, sizeof(buffer));
        if (!hash) {
            warnx("skipping %s: could not compute hash", fname);
            ++fails;
            continue;
        }

        printf("fname=%s hash=%s path=%s\n",
            fname, hash, OutDir_path(&outdir, hash)
        );
    }

    if (ioread.errno_save)
        ++fails;

exit:
    OutDir_free(&outdir);
    IORead_free(&ioread);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
