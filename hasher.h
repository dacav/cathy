#pragma once

typedef struct Hasher Hasher;

Hasher *Hasher_new(const char *hashprg);

const char * Hasher_hash_file(const Hasher *hash, const char *filename);

void Hasher_del(Hasher *hash);
