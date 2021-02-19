#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "filerepo.h"
#include "hasher.h"
#include "ioread.h"
#include "outdir.h"

typedef struct {
    const char *cmpprg;
    const char *hashprg;
    const char *outdir;
} Options;

static
void usage(const char *prgname, int exval)
{
    fprintf(stderr,
        "usage: %s"
        " [-C comparer]"
        " [-H hasher]"
        " [-o outdir]"
        "\n",
        prgname);
    exit(exval);
}

static
void parseopts(int argc, char **argv, Options *outops)
{
    int opt;

    *outops = (Options){
        .cmpprg = "cmp",
        .hashprg = "sha1sum",
        .outdir = ".",
    };

    while (opt = getopt(argc, argv, "C:hH:o:"), opt != -1) {
        switch (opt) {
        case 'C':
            outops->cmpprg = optarg;
            break;
        case 'H':
            outops->hashprg = optarg;
            break;
        case 'o':
            outops->outdir = optarg;
            break;
        default:
            usage(argv[0], opt == 'h' ? 0 : EX_USAGE);
        }
    }
}

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

int main(int argc, char **argv)
{
    Options opts;
    IORead ioread;
    Hasher *hash = NULL;
    FileRepo *filerepo = NULL;
    OutDir *outdir = NULL;
    int fails = 0;
    const char *fname;

    parseopts(argc, argv, &opts);

    hash = Hasher_new(opts.hashprg, opts.cmpprg);
    if (!hash) {
        ++fails;
        goto exit;
    }

    filerepo = FileRepo_new(hash);
    if (!filerepo) {
        ++fails;
        goto exit;
    }

    IORead_init(&ioread);
    while (fname = IORead_next(&ioread), fname != NULL)
        if (FileRepo_add(filerepo, fname)) {
            warnx("failure while handling %s", fname);
            ++fails;
        }
    if (ioread.errno_s)
        ++fails;

    outdir = OutDir_new(opts.outdir);
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
