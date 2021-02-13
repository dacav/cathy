#include "filerepo.h"

#include <err.h>
#include <stdbool.h>
#include <stdlib.h>

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
    time_t min_mtime;
    UT_hash_handle hh;
} Record;

struct FileRepo {
    Record *records;
    const Hash *hasher;
};

static
void Record_del(Record *record)
{
    PFile *pfile;

    if (!record)
        return;

    pfile = record->pfile;
    free(record);

    while (pfile) {
        PFile *next;

        File_free(&pfile->file);
        next = pfile->collisions.next;
        free(pfile);
        pfile = next;
    }
}

FileRepo *FileRepo_new(const Hash *hasher)
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
int FileRepo_attach_pfile(Record *record, PFile *pfile)
{
    pfile->collisions.next = record->pfile;
    record->pfile = pfile;

    if (pfile->file.mtime != record->min_mtime) {
        warnx("mtime discrepancy for hash '%s', taking smaller value",
            pfile->file.hash
        );

        if (pfile->file.mtime < record->min_mtime)
            record->min_mtime = pfile->file.mtime;
    }

    return 0;
}

static
int FileRepo_attach_record(FileRepo *filerepo, PFile *pfile)
{
    Record *record;

    HASH_FIND_STR(filerepo->records, pfile->file.hash, record);
    if (record)
        return FileRepo_attach_pfile(record, pfile);

    record = malloc(sizeof(Record));
    if (!record) {
        warn("malloc");
        return -1;
    }

    *record = (Record){
        .pfile = pfile,
        .min_mtime = pfile->file.mtime,
    };

    HASH_ADD_KEYPTR(
        hh,
        filerepo->records,
        pfile->file.hash,
        strlen(pfile->file.hash),
        record);

    return 0;
}

File *FileRepo_add(FileRepo *filerepo, const char *path)
{
    PFile *pfile;
    bool fileinit = false;
    const char *filehash;

    pfile = malloc(sizeof(PFile));
    if (!pfile) {
        warn("malloc");
        goto fail;
    }
    *pfile = (PFile){};

    filehash = Hash_file(filerepo->hasher, path);
    if (!filehash)
        goto fail;

    if (File_init(&pfile->file, path, filehash))
        goto fail;
    fileinit = true;

    if (FileRepo_attach_record(filerepo, pfile))
        goto fail;

    return &pfile->file;

fail:
    if (fileinit)
        File_free(&pfile->file);
    free(pfile);
    return NULL;
}

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
