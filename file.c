#include "file.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void File_free(File *file)
{
    free((void *)file->path);
}

int File_init(File *file, const char *path)
{
    struct stat statbuf;
    const char *abspath = NULL;

    if (stat(path, &statbuf) == -1) {
        warn("stat(%s, ...)", path);
        goto fail;
    }

    switch (statbuf.st_mode & S_IFMT) {
    case S_IFREG:
    case S_IFLNK:
        break;

    default:
        warnx("Skip '%s': not a (regular) file or softlink", path);
    case S_IFDIR:
        // directories are common, no need to log it.
        goto fail;
    }

    abspath = realpath(path, NULL);
    if (!abspath) {
        warn("realpath");
        goto fail;
    }

    *file = (File){
        .path = abspath,
        .mtime = statbuf.st_mtim.tv_sec
               + statbuf.st_mtim.tv_nsec / 1000000000,
        .device_id = statbuf.st_dev,
        .inode_id = statbuf.st_ino,
        .size = statbuf.st_size,
    };

    return 0;

fail:
    free((void *)abspath);
    return -1;
}

bool File_identical(File *f1, File *f2)
{
    return f1->inode_id == f2->inode_id
        && f1->device_id == f2->device_id;
}

void File_objswap(File *f1, File *f2)
{
    File aux;

    aux = *f1;
    *f1 = *f2;
    *f2 = aux;
}
