#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "outdir.h"
#include "util.h"

enum {
    OutDir_PREFIX = 5,
};

void OutDir_free(OutDir *outdir)
{
    Util_fdclose(&outdir->hashdir);
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

int OutDir_init(OutDir *outdir, const char *path)
{
    int basedir;

    *outdir = (OutDir){
        .hashdir = -1,
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

    Util_fdclose(&basedir);
    return 0;

fail:
    OutDir_free(outdir);
    Util_fdclose(&basedir);
    return -1;
}

static
int OutDir_hash_path(const OutDir *outdir, const char *hash, const char *path)
{
    char buffer[PATH_MAX];
    size_t hashlen;
    int dirfd;

    hashlen = strlen(hash);
    if (hashlen < OutDir_PREFIX || hashlen > sizeof(buffer) - 1) {
        warnx("hash for '%s' has unexpected length, %zu bytes", path, hashlen);
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
        warn("openat(%d, %s, O_DIRECTORY)", outdir->hashdir, buffer);
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
        Util_fdclose(&dirfd);
        return -1;
    }

    while (errno = 0, entry = readdir(dir))
        if (entry->d_type == DT_LNK)
            ++result;

    if (errno) {
        warn("readdir");
        result = -1;
    }
    if (closedir(dir))
        warn("closedir");

    return result;
}

int OutDir_link(const OutDir *outdir, const char *hash, const char *path)
{
    int dirfd = -1,
        ex = -1,
        links_count;
    char linkname[OutDir_PREFIX];
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

    snprintf(linkname, sizeof(linkname), "%*.*d", 0, (int)sizeof(linkname) - 1, links_count);
    if (symlinkat(abspath, dirfd, linkname))
        warn("symlinkat(%s, %d, %s)", path, dirfd, linkname);
    else
        ex = 0;
exit:
    Util_fdclose(&dirfd);
    return ex;
}
