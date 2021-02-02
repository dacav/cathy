#include "outdir.h"

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "util.h"

struct OutDir {
    int hashdir;
    int timedir;
};

enum {
    OutDir_PREFIX = 2,
};

void OutDir_del(OutDir *outdir)
{
    if (!outdir)
        return;

    Util_fdclose(&outdir->hashdir);
    Util_fdclose(&outdir->timedir);
    free(outdir);
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

OutDir *OutDir_new(const char *path)
{
    int basedir = -1;
    OutDir *outdir;

    outdir = malloc(sizeof(OutDir));
    if (!outdir) {
        warn("malloc");
        goto fail;
    }

    *outdir = (OutDir){
        .hashdir = -1,
        .timedir = -1,
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

    if (OutDir_mkdir(basedir, "by-time") == -1)
        goto fail;
    outdir->timedir = openat(basedir, "by-time", O_DIRECTORY);
    if (outdir->timedir == -1)
        goto fail;

    Util_fdclose(&basedir);
    return outdir;

fail:
    Util_fdclose(&basedir);
    OutDir_del(outdir);
    return NULL;
}

static
int OutDir_hash_path(const OutDir *outdir,
                     const OutDir_LinkInfo *linkinfo)
{
    char buffer[PATH_MAX];
    size_t hashlen;
    int dirfd;

    hashlen = strlen(linkinfo->hash);
    if (hashlen < OutDir_PREFIX || hashlen > sizeof(buffer) - 1) {
        warnx("hash for '%s' has unexpected length, %zu bytes",
              linkinfo->path,
              hashlen);
        return -1;
    }

    memcpy(buffer, linkinfo->hash, OutDir_PREFIX);
    buffer[OutDir_PREFIX] = '\0';
    if (OutDir_mkdir(outdir->hashdir, buffer))
        return -1;

    buffer[OutDir_PREFIX] = '/';
    memcpy(buffer + OutDir_PREFIX + 1,
           linkinfo->hash + OutDir_PREFIX,
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
int OutDir_time_path(const OutDir *outdir,
                     const OutDir_LinkInfo *linkinfo)
{
    char buffer[PATH_MAX];
    int dirfd;
    struct tm tm;

    if (gmtime_r(&linkinfo->mtime, &tm) == NULL) {
        warn("gmtime_r(&{%ld}, ...)", linkinfo->mtime);
        return -1;
    }

    if (strftime(buffer, sizeof(buffer), "%Y/%m/%d", &tm) == 0) {
        warnx("strftime failed");
        return -1;
    }

    buffer[4] = '\0';
    if (OutDir_mkdir(outdir->timedir, buffer))
        return -1;
    buffer[4] = '/';

    buffer[7] = '\0';
    if (OutDir_mkdir(outdir->timedir, buffer))
        return -1;
    buffer[7] = '/';

    if (OutDir_mkdir(outdir->timedir, buffer))
        return -1;

    dirfd = openat(outdir->timedir, buffer, O_DIRECTORY);
    if (dirfd == -1)
        warn("openat(%d, %s, O_DIRECTORY)", outdir->timedir, buffer);
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

static
int OutDir_link_under(int dirfd, const char *target)
{
    int links_count;
    char linkname[11 /* enough for int */];

    links_count = OutDir_count_links(dirfd);
    if (links_count == -1)
        return -1;

    snprintf(
        linkname,
        sizeof(linkname),
        "%.*d",
        OutDir_PREFIX - 1,
        links_count);

    if (symlinkat(target, dirfd, linkname)) {
        warn("symlinkat(%s, %d, %s)", target, dirfd, linkname);
        return -1;
    }

    return 0;
}

int OutDir_link(const OutDir *outdir, const OutDir_LinkInfo *linkinfo)
{
    int dirfd = -1, ex = -1;

    dirfd = OutDir_hash_path(outdir, linkinfo);
    if (dirfd == -1)
        goto exit;
    if (OutDir_link_under(dirfd, linkinfo->path))
        goto exit;

    Util_fdclose(&dirfd);
    dirfd = OutDir_time_path(outdir, linkinfo);
    if (dirfd == -1)
        goto exit;
    if (OutDir_link_under(dirfd, linkinfo->path))
        goto exit;

    ex = 0;
exit:
    Util_fdclose(&dirfd);
    return ex;
}
