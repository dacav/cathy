#pragma once

#include <stddef.h>

typedef struct {
    char *buffer;
    size_t buflen;
    int errno_s;
} IORead;

void IORead_init(IORead *ioread);

void IORead_free(IORead *ioread);

const char *IORead_next(IORead *ioread);
