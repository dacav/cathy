#include "file.h"

#include <stdlib.h>
#include <string.h>
#include <err.h>

void File_free(File *file)
{
    free((void *)file->path);
    free((void *)file->hash);
}

int File_init(File *file, const char *path, const char *hash)
{
    file->path = strdup(path);
    if (!file->path) {
        warn("strdup");
        goto fail;
    }

    file->hash = strdup(hash);
    if (!file->hash){
        warn("strdup");
        goto fail;
    }

    return 0;

fail:
    File_free(file);
    return -1;
}
