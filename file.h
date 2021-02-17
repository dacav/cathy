#pragma once

#include <time.h>

typedef struct File {
    const char *path;
    time_t mtime;
} File;

int File_init(File *, const char *path);

void File_objswap(File *, File *);

void File_free(File *);
