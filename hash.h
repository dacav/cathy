#pragma once

typedef struct Hash Hash;

Hash *Hash_new(const char *hashprg);

const char * Hash_file(Hash *hash, const char *filename);

void Hash_del(Hash *hash);
