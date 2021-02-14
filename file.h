#pragma once

#include <time.h>

typedef struct File {
    const char *path;
    const char *hash;
    time_t mtime;
} File;

int File_init(File *, const char *path, const char *hash);
void File_free(File *);
