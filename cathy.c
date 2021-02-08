#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <limits.h>

#include "hash.h"
#include "ioread.h"
#include "outdir.h"
#include "util.h"

/* -- main ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    IORead ioread;
    OutDir outdir;
    int fails = 0;

    const char *outdir_path = "./cathy.d"; // TODO: argv

    IORead_init(&ioread);

    if (OutDir_init(&outdir, outdir_path)) {
        ++fails;
        goto exit;
    }

    const char *fname;
    while (fname = IORead_next(&ioread), fname != NULL) {
        char buffer[Hash_buflen];
        const char *hash;

        hash = Hash_file(fname, buffer, sizeof(buffer));
        if (!hash) {
            warnx("skipping %s: could not compute hash", fname);
            ++fails;
            continue;
        }

        if (OutDir_link(&outdir, hash, fname))
            ++fails;
    }

    if (ioread.errno_s)
        ++fails;

exit:
    OutDir_free(&outdir);
    IORead_free(&ioread);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
