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

#define File_FMT "File['%s', mtime=%ld size=%zu]"
#define File_REPR(f) (f)->path, (f)->mtime, (f)->size

int File_init(File *, const char *path);

bool File_identical(File *, File *);

void File_objswap(File *, File *);

void File_free(File *);
