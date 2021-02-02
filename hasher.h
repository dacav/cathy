#pragma once

#include <stdbool.h>

typedef struct Hasher Hasher;

Hasher *Hasher_new(const char *hashprg, const char *compprg);

const char * Hasher_hash_file(const Hasher *hash, const char *path);
int Hasher_comp_files(const Hasher *hash,
                      const char *path1,
                      const char *path2,
                      bool *equals);

void Hasher_del(Hasher *hash);
