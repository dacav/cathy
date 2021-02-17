#include "filerepo.h"

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sysexits.h>

#include <uthash.h>
#include <utlist.h>

#define debug(...) warnx(__VA_ARGS__)

typedef struct PFile {
    File file;
    struct PFile *next;
} PFile;

typedef struct {
    PFile *unique_files;
    const char *filehash;
    UT_hash_handle hh;
} Record;

struct FileRepo {
    Record *records;
    const Hasher *hasher;
    PFile *removals;
};

static
PFile *PFile_new(const char *path)
{
    PFile *pfile;

    pfile = malloc(sizeof(PFile));
    if (!pfile) {
        warn("malloc");
        goto fail;
    }
    *pfile = (PFile){};

    if (File_init(&pfile->file, path))
        goto fail;

    return pfile;

fail:
    // NOTE: skipped call to File_free, as File_init is the last call
    //  before returning.
    free(pfile);
    return NULL;
}

static
void PFile_del(PFile *pfile)
{
    if (!pfile)
        return;

    File_free(&pfile->file);
    free(pfile);
}

static
void Record_del(Record *record)
{
    PFile *pfile, *tmp;

    if (!record)
        return;

    pfile = record->unique_files;
    LL_FOREACH_SAFE(record->unique_files, pfile, tmp) {
        LL_DELETE(record->unique_files, pfile);
        PFile_del(pfile);
    }

    free((void *)record->filehash);
    free(record);
}

FileRepo *FileRepo_new(const Hasher *hasher)
{
    FileRepo *filerepo;

    filerepo = malloc(sizeof(FileRepo));
    if (!filerepo) {
        warn("malloc");
        goto fail;
    }

    *filerepo = (FileRepo){
        .hasher = hasher,
    };

    return filerepo;

fail:
    FileRepo_del(filerepo);
    return NULL;
}

static
int FileRepo_handle_duplicate(FileRepo *filerepo,
                              PFile *pfile,
                              PFile *duplicate)
{
    // We want to suggest the removal of one of the two duplicates.
    // From the data perspective it doesn't matter which one, but the
    // file modification time (mtime) might be different.
    // If this is the case, we want to keep the oldest file, since the
    // most recent copy is likely to be wrong (unless the clock was
    // in the past for the host that made the copy.

    if (duplicate->file.mtime < pfile->file.mtime)
        File_objswap(&pfile->file, &duplicate->file);

    debug("Would remove duplicate: %s", duplicate->file.path);
    debug(" keep: %s (mtime=%ld)", pfile->file.path, pfile->file.mtime);
    debug(" drop: %s (mtime=%ld)", duplicate->file.path, duplicate->file.mtime);
    LL_PREPEND(filerepo->removals, duplicate);
    return 0;
}

static
int FileRepo_attach_pfile(FileRepo *filerepo,
                          Record *record,
                          PFile *new_pfile)
{
    PFile *pfile;

    LL_FOREACH(record->unique_files, pfile) {
        bool is_copy;
        if (Hasher_comp_files(
                filerepo->hasher,
                pfile->file.path,
                new_pfile->file.path,
                &is_copy) == -1)
            return -1;

        if (is_copy)
            return FileRepo_handle_duplicate(filerepo, pfile, new_pfile);
    }

    LL_PREPEND(record->unique_files, new_pfile);
    return 0;
}

static
int FileRepo_attach_record(FileRepo *filerepo,
                           const char *filehash,
                           PFile *pfile)
{
    Record *record;

    HASH_FIND_STR(filerepo->records, filehash, record);
    if (record)
        return FileRepo_attach_pfile(filerepo, record, pfile);

    record = malloc(sizeof(Record));
    if (!record) {
        warn("malloc");
        return -1;
    }

    *record = (Record){
        .unique_files = pfile,
        .filehash = strdup(filehash),
    };

    if (!record->filehash) {
        warn("strdup");
        return -1;
    }

    HASH_ADD_KEYPTR(
        hh,
        filerepo->records,
        record->filehash,
        strlen(record->filehash),
        record);

    return 0;
}

int FileRepo_add(FileRepo *filerepo, const char *path)
{
    PFile *pfile = NULL;
    const char *filehash;

    filehash = Hasher_hash_file(filerepo->hasher, path);
    if (!filehash)
        goto fail;

    pfile = PFile_new(path);
    if (!pfile)
        goto fail;

    if (FileRepo_attach_record(filerepo, filehash, pfile))
        goto fail;

    return 0;

fail:
    PFile_del(pfile);
    return -1;
}

#if 0
const File *FileRepo_iter(const FileRepo *filerepo, void **aux)
{
    typedef struct {
        const Record *record;
        const PFile *pfile;
        File file;
    } Iter;

    Iter *iter = *aux;
    const Record *record;

    if (iter == NULL) {
        if (!filerepo->records)
            return NULL;    // just empty.

        iter = *aux = malloc(sizeof(Iter));
        if (!iter) {
            warn("malloc failed, iteration yields nothing");
            return NULL;
        }
        record = filerepo->records;
    }
    else if (!iter->pfile) {
        if (!iter->record) {
            // cannot advance, reached end of iteration
            free(iter);
            return *aux = NULL;
        }

        record = iter->record->hh.next;
    }
    else
        record = iter->record;

    if (!record->pfile)
        err(EX_SOFTWARE, "buggy! empty record");

    *iter = (Iter) {
        .record = record,
        .pfile = record->pfile,
        .file.path = iter->pfile->file.path,
        .file.hash = iter->pfile->file.hash,
        .file.mtime = iter->record->min_mtime,
    };
    iter->pfile = iter->pfile->collisions.next;

    return &iter->file;
}
#endif

void FileRepo_del(FileRepo *filerepo)
{
    Record *record, *tmp1;
    PFile *pfile, *tmp2;

    if (!filerepo)
        return;

    HASH_ITER(hh, filerepo->records, record, tmp1) {
        HASH_DEL(filerepo->records, record);
        Record_del(record);
    }

    LL_FOREACH_SAFE(filerepo->removals, pfile, tmp2) {
        LL_DELETE(filerepo->removals, pfile);
        PFile_del(pfile);
    }

    free(filerepo);
}
