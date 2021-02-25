#include <err.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#include "events.h"
#include "file.h"
#include "filerepo.h"
#include "hasher.h"
#include "ioread.h"
#include "outdir.h"

typedef struct {
    const char *cmpprg;
    const char *hashprg;
    const char *outdir;
    const char *events_logfile;
    bool remove_files;
} Options;

static
void usage(const char *prgname, int exval)
{
    fprintf(stderr,
        "usage: %s"
        " [-C comparer]"
        " [-e events_log_file]"
        " [-H hasher]"
        " [-o outdir]"
        " [-r]"
        "\n",
        prgname);
    exit(exval);
}

static
void parseopts(int argc, char **argv, Options *outopts)
{
    int opt;

    *outopts = (Options){
        .cmpprg = "cmp",
        .hashprg = "sha1sum",
        .outdir = ".",
    };

    while (opt = getopt(argc, argv, "C:e:hH:o:r"), opt != -1) {
        switch (opt) {
        case 'C':
            outopts->cmpprg = optarg;
            break;
        case 'e':
            outopts->events_logfile = optarg;
            break;
        case 'H':
            outopts->hashprg = optarg;
            break;
        case 'o':
            outopts->outdir = optarg;
            break;
        case 'r':
            outopts->remove_files = true;
            break;
        default:
            usage(argv[0], opt == 'h' ? 0 : EX_USAGE);
        }
    }
}

static
void loop_entries(const FileRepo *filerepo,
                  const OutDir *outdir,
                  Events *events)
{
    void *aux = NULL;
    const FileRepo_Entry *entry;

    while (entry = FileRepo_iter(filerepo, &aux), entry != NULL) {
        if (OutDir_link(outdir, &(OutDir_LinkInfo){
                .hash = entry->filehash,
                .path = entry->file->path,
                .mtime = entry->file->mtime,
            })
        ) {
            warnx("failed to link file %s (filehash %s)",
                entry->file->path,
                entry->filehash);
            continue;
        }

        Events_accept_file(events, entry->file);
    }
}

static
void loop_removals(const FileRepo *filerepo,
                   Events *events,
                   bool remove_files)
{
    void *aux = NULL;
    const File *file;

    while (file = FileRepo_iter_removals(filerepo, &aux), file != NULL) {
        Events_reject_file(events, file);
        if (remove_files)
            unlink(file->path);
    }
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
    Events *events = NULL;

    parseopts(argc, argv, &opts);

    events = Events_new(opts.events_logfile);
    if (!events) {
        ++fails;
        goto exit;
    }

    hash = Hasher_new(opts.hashprg, opts.cmpprg);
    if (!hash) {
        ++fails;
        goto exit;
    }

    filerepo = FileRepo_new(hash, events);
    if (!filerepo) {
        ++fails;
        goto exit;
    }

    IORead_init(&ioread);
    while (fname = IORead_next(&ioread), fname != NULL)
        if (FileRepo_add(filerepo, fname)) {
            Events_skipped_filename(events, fname);
            ++fails;
        }
    if (ioread.errno_s)
        ++fails;

    outdir = OutDir_new(opts.outdir);
    if (!outdir) {
        ++fails;
        goto exit;
    }

    loop_entries(filerepo, outdir, events);
    loop_removals(filerepo, events, opts.remove_files);

    Events_print_stats(events, !opts.remove_files);

exit:
    OutDir_del(outdir);
    FileRepo_del(filerepo);
    Hasher_del(hash);
    IORead_free(&ioread);
    Events_del(events);
    return fails ? EXIT_FAILURE : EXIT_SUCCESS;
}
