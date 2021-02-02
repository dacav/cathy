#include <err.h>
#include <unistd.h>

int Util_fdclose(int *fdptr)
{
    int fd = *fdptr;
    *fdptr = -1;

    if (fd != -1 && close(fd)) {
        warn("close(%d)", fd);
        return -1;
    }
    return 0;
}
