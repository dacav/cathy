#pragma once

#include "file.h"
#include "hasher.h"

typedef struct FileRepo FileRepo;

FileRepo *FileRepo_new(const Hasher *);

File *FileRepo_add(FileRepo *, const char *path);

void FileRepo_del(FileRepo *);
