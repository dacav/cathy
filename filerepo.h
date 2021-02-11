#pragma once

#include "file.h"
#include "hash.h"

typedef struct FileRepo FileRepo;

FileRepo *FileRepo_new(const Hash *);

File *FileRepo_add(FileRepo *, const char *path);

void FileRepo_del(FileRepo *);
