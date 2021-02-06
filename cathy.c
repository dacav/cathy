#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

/* -- miscellaneous --------------------------------------------------- */

static int fdclose(int *fdptr)
{
    int fd = *fdptr;
    *fdptr = -1;

    if (fd != -1 && close(fd)) {
        warn("close(%d)", fd);
        return -1;
    }
    return 0;
}

/* -- IORead ---------------------------------------------------------- */

typedef struct {
    char *buffer;
    size_t buflen;
    int errno_s;
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
        ioread->errno_s = errno;
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
    int hashdir;
    size_t hashlen;
} OutDir;

static
void OutDir_free(OutDir *outdir)
{
    fdclose(&outdir->hashdir);
}

static
int OutDir_mkdir(int dirfd, const char *path)
{
    if (mkdirat(dirfd, path, 0777) && errno != EEXIST) {
        warn("mkdirat(%d, %s, 0777)", dirfd, path);
        return -1;
    }
    return 0;
}

static
int OutDir_init(OutDir *outdir, const char *path, size_t hashlen)
{
    int basedir;

    *outdir = (OutDir){
        .hashdir = -1,
        .hashlen = hashlen,
    };

    if (OutDir_mkdir(AT_FDCWD, path) == -1)
        goto fail;

    basedir = openat(AT_FDCWD, path, O_DIRECTORY);
    if (basedir == -1)
        goto fail;

    if (OutDir_mkdir(basedir, "by-hash") == -1)
        goto fail;

    outdir->hashdir = openat(basedir, "by-hash", O_DIRECTORY);
    if (outdir->hashdir == -1)
        goto fail;

    fdclose(&basedir);
    return 0;

fail:
    OutDir_free(outdir);
    fdclose(&basedir);
    return -1;
}

static
int OutDir_readlink(char *dst,
                    size_t dstlen,
                    int dirfd,
                    const char *linkname)
{
    ssize_t len;

    len = readlinkat(dirfd, linkname, dst, dstlen);
    if (len == -1) {
        warn("readlinkat(%d, %s, ...)", dirfd, linkname);
        return -1;
    }
    if (len == dstlen) {
        warnx("readlinkat(%d, %s, ...) truncated", dirfd, linkname);
        return -1;
    }
    dst[len] = '\0';
    return 0;
}

static
int OutDir_hash_path(const OutDir *outdir, const char *hash, const char *path)
{
    char buffer[PATH_MAX];
    size_t hashlen;
    int dirfd;

    hashlen = strlen(hash);
    if (hashlen < OutDir_PREFIX || hashlen > sizeof(buffer) - 1) {
        warnx("hash for '%s' has unexpected length, %zu bytes", hashlen);
        return -1;
    }

    memcpy(buffer, hash, OutDir_PREFIX);
    buffer[OutDir_PREFIX] = '\0';
    if (OutDir_mkdir(outdir->hashdir, buffer))
        return -1;

    buffer[OutDir_PREFIX] = '/';
    memcpy(buffer + OutDir_PREFIX + 1,
           hash + OutDir_PREFIX,
           hashlen - OutDir_PREFIX - 1);
    buffer[hashlen] = '\0';
    if (OutDir_mkdir(outdir->hashdir, buffer))
        return -1;

    dirfd = openat(outdir->hashdir, buffer, O_DIRECTORY);
    if (dirfd == -1)
        warn("openat(%s, %s, O_DIRECTORY)", outdir->hashdir, buffer);
    return dirfd;
}

static
int OutDir_count_links(int dirfd)
{
    DIR *dir;
    struct dirent *entry;
    int result = 0;

    dirfd = dup(dirfd);
    if (dirfd == -1) {
        warn("dup");
        return -1;
    }

    dir = fdopendir(dirfd);
    if (!dir) {
        warn("fdopendir");
        fdclose(&dirfd);
        return -1;
    }

    while (errno = 0, entry = readdir(dir))
        if (entry->d_type == DT_LNK)
            ++result;

    if (errno) {
        warn("readdir");
        result = -1;
    }
    closedir(dir);

    return result;
}

static
int OutDir_link(const OutDir *outdir, const char *hash, const char *path)
{
    int dirfd = -1,
        ex = -1,
        links_count;
    char linkname[4];
    char abspath[PATH_MAX];

    dirfd = OutDir_hash_path(outdir, hash, path);
    if (dirfd == -1)
        goto exit;

    links_count = OutDir_count_links(dirfd);
    if (links_count == -1)
        goto exit;

    if (realpath(path, abspath) == NULL) {
        warn("realpath(%s, ...)", path);
        goto exit;
    }

    snprintf(linkname, sizeof(linkname), "%03d", links_count);
    if (symlinkat(abspath, dirfd, linkname))
        warn("symlinkat(%s, %d, %s)", path, dirfd, linkname);
    else
        ex = 0;
exit:
    fdclose(&dirfd);
    return ex;
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
    if (fdclose(&r))
        exit(EXIT_FAILURE);
    if (dup2(w, STDOUT_FILENO) == -1)
        err(1, "dup2(%d, %d)", w, STDOUT_FILENO);
    if (fdclose(&w) == -1)
        exit(EXIT_FAILURE);
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

    if (fdclose(&pipefd[w]) == -1)
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

    fdclose(&pipefd[r]);

    buffer[nread - 1] = '\0';
    return buffer;
    
fail:
    if (pid > 0)
        Hash_wait(pid);

    fdclose(&pipefd[r]);
    fdclose(&pipefd[w]);
    return NULL;
}


/* -- main ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    IORead ioread;
    OutDir outdir;
    int fails = 0;

    const char *outdir_path = "./cathy.d"; // TODO: argv

    IORead_init(&ioread);

    if (OutDir_init(&outdir, outdir_path, Hash_checksum_length)) {
        ++fails;
        goto exit;
    }

    const char *fname;
    while (fname = IORead_next(&ioread), fname != NULL) {
        char buffer[Hash_buflen];
        const char *hash;

        hash = Hash_file(fname, buffer, sizeof(buffer));
        if (!hash) {
            warnx("skipping %s: could not compute hash", fname);
            ++fails;
            continue;
        }

        if (OutDir_link(&outdir, hash, fname))
            ++fails;
    }

    if (ioread.errno_s)
        ++fails;

exit:
    OutDir_free(&outdir);
    IORead_free(&ioread);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
