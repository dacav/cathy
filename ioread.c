#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "ioread.h"

void IORead_init(IORead *ioread)
{
    *ioread = (IORead) {};
}

void IORead_free(IORead *ioread)
{
    free(ioread->buffer);
}

const char *IORead_next(IORead *ioread)
{
    enum {
        delm = 0,
    };

    ssize_t nread;

    errno = 0;
    nread = getdelim(&ioread->buffer, &ioread->buflen, delm, stdin);

    if (nread == -1) {
        ioread->errno_s = errno;
        if (errno)
            warn("getdelim");
        return NULL;
    }

    ioread->buffer[nread - 1] = '\0';
    return ioread->buffer;
}

