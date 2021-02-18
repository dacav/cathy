#pragma once

#include <stdbool.h>
#include <sys/stat.h>
#include <time.h>

typedef struct File {
    const char *path;
    time_t mtime;
    ino_t inode_id;
} File;

int File_init(File *, const char *path);

bool File_identical(File *, File *);

void File_objswap(File *, File *);

void File_free(File *);
