#pragma once

typedef struct {
    const char *hashprg;
    char *buffer;
} Hash;

int Hash_init(Hash *hash, const char *hashprg);

const char * Hash_file(Hash *hash, const char *filename);

void Hash_free(Hash *hash);
