#pragma once

#include "file.h"
#include "hasher.h"

typedef struct FileRepo FileRepo;
typedef struct {
    const File *file;
    const char *filehash;
} FileRepo_Entry;

struct Events;

FileRepo *FileRepo_new(const Hasher *, struct Events *);

const FileRepo_Entry *FileRepo_iter(const FileRepo *, void **aux);
const File *FileRepo_iter_removals(const FileRepo *, void **aux);

int FileRepo_add(FileRepo *, const char *path);

void FileRepo_del(FileRepo *);
