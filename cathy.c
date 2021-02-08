#include <err.h>
#include <stdlib.h>

#include "hash.h"
#include "ioread.h"
#include "outdir.h"
#include "util.h"

/* -- main ------------------------------------------------------------ */

int main(int argc, char **argv)
{
    IORead ioread;
    OutDir outdir;
    Hash hash;
    int fails = 0;

    const char *outdir_path = "./cathy.d"; // TODO: argv
    const char *hashprg = "md5sum";

    IORead_init(&ioread);

    if (Hash_init(&hash, hashprg)) {
        ++fails;
        goto exit;
    }

    if (OutDir_init(&outdir, outdir_path)) {
        ++fails;
        goto exit;
    }

    const char *fname;
    while (fname = IORead_next(&ioread), fname != NULL) {
        const char *filehash;

        filehash = Hash_file(&hash, fname);
        if (!filehash) {
            warnx("skipping %s: could not compute hash", fname);
            ++fails;
            continue;
        }

        if (OutDir_link(&outdir, filehash, fname))
            ++fails;
    }

    if (ioread.errno_s)
        ++fails;

exit:
    OutDir_free(&outdir);
    IORead_free(&ioread);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
