#include "filerepo.h"

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sysexits.h>
#include <uthash.h>
#include <utlist.h>
#include <string.h>

#include "events.h"

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
    Events *events;
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

FileRepo *FileRepo_new(const Hasher *hasher, Events *events)
{
    FileRepo *filerepo;

    filerepo = malloc(sizeof(FileRepo));
    if (!filerepo) {
        warn("malloc");
        goto fail;
    }

    *filerepo = (FileRepo){
        .hasher = hasher,
        .events = events,
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
    // We want to remove one of the two duplicates.
    // From the data perspective it doesn't matter which one, but the
    // file modification time (mtime) might be different.  If this is
    // the case, we want to keep the oldest file, since the most
    // recent copy is likely to be wrong (unless the clock was in the
    // past for the host that made the copy.
    if (duplicate->file.mtime < pfile->file.mtime)
        File_objswap(&pfile->file, &duplicate->file);

    Events_duplicate(filerepo->events, &pfile->file, &duplicate->file);
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

        if (File_identical(&pfile->file, &new_pfile->file)) {
            Events_ignored_identical(filerepo->events, &pfile->file,
                &new_pfile->file);
            PFile_del(new_pfile);
            return 0;
        }

        if (Hasher_comp_files(
                filerepo->hasher,
                pfile->file.path,
                new_pfile->file.path,
                &is_copy) == -1)
            return -1;

        if (is_copy)
            return FileRepo_handle_duplicate(filerepo, pfile, new_pfile);
    }

    Events_collision(filerepo->events, &new_pfile->file, record->filehash);
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
        goto fail;
    }

    *record = (Record){
        .unique_files = pfile,
        .filehash = strdup(filehash),
    };

    if (!record->filehash) {
        warn("strdup");
        goto fail;
    }

    HASH_ADD_KEYPTR(
        hh,
        filerepo->records,
        record->filehash,
        strlen(record->filehash),
        record);

    return 0;

fail:
    if (record) {
        free((void *)record->filehash);
        free(record);
    }
    return -1;
}

int FileRepo_add(FileRepo *filerepo, const char *path)
{
    PFile *pfile = NULL;
    const char *filehash;

    pfile = PFile_new(path);
    if (!pfile)
        goto fail;

    filehash = Hasher_hash_file(filerepo->hasher, path);
    if (!filehash)
        goto fail;

    if (FileRepo_attach_record(filerepo, filehash, pfile))
        goto fail;

    return 0;

fail:
    PFile_del(pfile);
    return -1;
}

const FileRepo_Entry *FileRepo_iter(const FileRepo *filerepo, void **aux)
{
    typedef struct {
        const Record *record;
        const PFile *pfile;
        FileRepo_Entry entry;
    } Iter;

    Iter *iter = *aux;

    if (iter == NULL) {
        if (!filerepo->records)
            return NULL;    // Empty. Stop immediately.

        iter = *aux = malloc(sizeof(Iter));
        if (!iter) {
            warn("malloc failed, iteration yields nothing");
            return NULL;
        }

        // Take the first record, and the first of the files of
        // the record.
        *iter = (Iter){
            .record = filerepo->records,
            .pfile = filerepo->records->unique_files,
        };
    }
    else if (iter->pfile->next) {
        iter->pfile = iter->pfile->next;
    } else {
        // Advance record, take the first pfile of the record.
        iter->record = iter->record->hh.next;

        if (!iter->record) {
            // Cannot advance, reached end of iteration.
            free(iter);
            return *aux = NULL;
        }
        iter->pfile = iter->record->unique_files;
    }

    if (!iter->pfile || !iter->record)
        errx(EX_SOFTWARE, "buggy! %p %p", iter->pfile, iter->record);

    iter->entry = (FileRepo_Entry){
        .file = &iter->pfile->file,
        .filehash = iter->record->filehash,
    };
    return &iter->entry;
}

const File *FileRepo_iter_removals(const FileRepo *filerepo,
                                             void **aux)
{
    PFile *pfile;

    pfile = *aux;
    if (!pfile)
        pfile = filerepo->removals;
    else
        pfile = pfile->next;
    *aux = pfile;
    return &pfile->file;
}

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
