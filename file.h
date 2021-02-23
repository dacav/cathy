#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <sys/stat.h>
#include <time.h>

typedef struct File {
    const char *path;
    time_t mtime;
    dev_t device_id;
    ino_t inode_id;
    off_t size;
} File;

int File_init(File *, const char *path);

bool File_identical(File *, File *);

void File_objswap(File *, File *);

void File_free(File *);
