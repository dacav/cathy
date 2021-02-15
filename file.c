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
int File_load_timestamp(File *file)
{
    struct stat statbuf;

    if (stat(file->path, &statbuf) == -1) {
        warn("stat(%s, ...)", file->path);
        return -1;
    }

    file->mtime = statbuf.st_mtim.tv_sec
                + statbuf.st_mtim.tv_nsec / 1000000000;
    return 0;
}

int File_init(File *file, const char *path)
{
    file->path = strdup(path);
    if (!file->path) {
        warn("strdup");
        goto fail;
    }

    if (File_load_timestamp(file))
        goto fail;

    return 0;

fail:
    File_free(file);
    return -1;
}
