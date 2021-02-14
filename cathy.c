#include <err.h>
#include <stdlib.h>

#include "filerepo.h"
#include "hasher.h"
#include "ioread.h"
#include "outdir.h"

int main(void)
{
    IORead ioread;
    Hasher *hash = NULL;
    FileRepo *filerepo = NULL;
    OutDir outdir;
    int fails = 0;

    const char *outdir_path = "./cathy.d"; // TODO: argv
    const char *hashprg = "md5sum";
    const char *cmpprog = "cmp";

    IORead_init(&ioread);

    hash = Hasher_new(hashprg, cmpprog);
    if (!hash) {
        ++fails;
        goto exit;
    }

    filerepo = FileRepo_new(hash);
    if (!filerepo) {
        ++fails;
        goto exit;
    }

    if (OutDir_init(&outdir, outdir_path)) {
        ++fails;
        goto exit;
    }

    const char *fname;
    while (fname = IORead_next(&ioread), fname != NULL)
        if (FileRepo_add(filerepo, fname)) {
            warnx("failure handling %s", fname);
            ++fails;
        }

    if (ioread.errno_s)
        ++fails;

exit:
    OutDir_free(&outdir);
    FileRepo_del(filerepo);
    Hasher_del(hash);
    IORead_free(&ioread);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
