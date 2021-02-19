#include <err.h>
#include <stdlib.h>

#include "filerepo.h"
#include "hasher.h"
#include "ioread.h"
#include "outdir.h"

static
void loop_entries(const FileRepo *filerepo, const OutDir *outdir)
{
    void *aux = NULL;
    const FileRepo_Entry *entry;

    while (entry = FileRepo_iter(filerepo, &aux), entry != NULL)
        if (OutDir_link(outdir, entry->filehash, entry->file->path))
            warnx("failed to categorize file %s (filehash %s)",
                entry->file->path,
                entry->filehash);
}

static
void loop_removals(const FileRepo *filerepo)
{
    void *aux = NULL;
    const File *file;

    while (file = FileRepo_iter_removals(filerepo, &aux), file != NULL)
        warnx("remove file %s", file->path);
}

int main(void)
{
    IORead ioread;
    Hasher *hash = NULL;
    FileRepo *filerepo = NULL;
    OutDir *outdir = NULL;
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

    const char *fname;
    while (fname = IORead_next(&ioread), fname != NULL)
        if (FileRepo_add(filerepo, fname)) {
            warnx("failure while handling %s", fname);
            ++fails;
        }
    if (ioread.errno_s)
        ++fails;

    outdir = OutDir_new(outdir_path);
    if (!outdir) {
        ++fails;
        goto exit;
    }

    loop_entries(filerepo, outdir);
    loop_removals(filerepo);

exit:
    OutDir_del(outdir);
    FileRepo_del(filerepo);
    Hasher_del(hash);
    IORead_free(&ioread);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
