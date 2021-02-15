#include "filerepo.h"

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>
#include <sysexits.h>

#include <uthash.h>

#define debug(...) warnx(__VA_ARGS__)

typedef struct PFile {
    File file;
    struct {
        struct PFile *next;
    } collisions;
} PFile;

typedef struct {
    PFile *pfile;
    const char *filehash;
    time_t min_mtime;
    UT_hash_handle hh;
} Record;

struct FileRepo {
    Record *records;
    const Hasher *hasher;
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
    PFile *pfile;

    if (!record)
        return;

    pfile = record->pfile;
    free((void *)record->filehash);
    free(record);

    while (pfile) {
        PFile *next;

        next = pfile->collisions.next;
        PFile_del(pfile);
        pfile = next;
    }
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
int FileRepo_handle_duplicate(Record *record,
                              PFile *pfile)
{
    warnx("duplicate detected: f1 %s f2 %s hash %s.",
        record->pfile->file.path,
        pfile->file.path,
        record->filehash
    );

    if (pfile->file.mtime != record->min_mtime) {
        warnx(" mtime discrepancy for duplicate, taking smaller value.");

        if (pfile->file.mtime < record->min_mtime)
            record->min_mtime = pfile->file.mtime;
    }

    PFile_del(pfile);
    return 0;
}

static
int FileRepo_attach_pfile(FileRepo *filerepo, Record *record, PFile *pfile)
{
    PFile * const head = record->pfile;

    if (head) {
        bool is_copy;
        if (Hasher_comp_files(
                filerepo->hasher,
                head->file.path,
                pfile->file.path,
                &is_copy) == -1)
            return -1;

        if (is_copy)
            return FileRepo_handle_duplicate(record, pfile);
    }

    pfile->collisions.next = head;
    record->pfile = pfile;

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
        .pfile = pfile,
        .filehash = strdup(filehash),
        .min_mtime = pfile->file.mtime,
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
    Record *record, *tmp;

    if (!filerepo)
        return;

    HASH_ITER(hh, filerepo->records, record, tmp) {
        HASH_DEL(filerepo->records, record);
        Record_del(record);
    }

    free(filerepo);
}
