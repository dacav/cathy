#include "file.h"

#include <err.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

void File_free(File *file)
{
    free((void *)file->path);
}

static
int File_load_metadata(File *file)
{
    struct stat statbuf;

    if (stat(file->path, &statbuf) == -1) {
        warn("stat(%s, ...)", file->path);
        return -1;
    }

    file->mtime = statbuf.st_mtim.tv_sec
                + statbuf.st_mtim.tv_nsec / 1000000000;
    file->inode_id = statbuf.st_ino;
    return 0;
}

int File_init(File *file, const char *path)
{
    file->path = strdup(path);
    if (!file->path) {
        warn("strdup");
        goto fail;
    }

    if (File_load_metadata(file))
        goto fail;

    return 0;

fail:
    File_free(file);
    return -1;
}

bool File_identical(File *f1, File *f2)
{
    return f1->inode_id == f2->inode_id;
}

void File_objswap(File *f1, File *f2)
{
    File aux;

    aux = *f1;
    *f1 = *f2;
    *f2 = aux;
}
