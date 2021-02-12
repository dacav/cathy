#pragma once

#include <time.h>

typedef struct File {
    const char *path;
    const char *hash;

    /* Optional fields, populated on request */
    time_t timestamp;
} File;

int File_init(File *, const char *path, const char *hash);
void File_free(File *);
