enum {
    Hash_checksum_length = 32,
    Hash_buflen = Hash_checksum_length + 1,
};

const char * Hash_file(const char *filename, char *buffer, size_t buflen);
