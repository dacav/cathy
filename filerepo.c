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
    UT_hash_handle hh;
} PFile;

struct FileRepo {
    PFile *files;
    const Hash *hasher;
};

static
void PFile_del(PFile *pfile)
{
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
int FileRepo_attach(FileRepo *filerepo, PFile *pfile)
{
    PFile *aux;

    HASH_FIND_STR(filerepo->files, pfile->file.hash, aux);
    if (aux) {
        debug("seen hash %s (file %s)", pfile->file.hash, pfile->file.path);
        pfile->collisions.next = aux->collisions.next;
        aux->collisions.next = pfile;
        return 0;
    }

    debug("unseen hash %s (file %s)", pfile->file.hash, pfile->file.path);
    HASH_ADD_KEYPTR(
        hh,
        filerepo->files,
        pfile->file.hash,
        strlen(pfile->file.hash),
        pfile);

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

    if (FileRepo_attach(filerepo, pfile))
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
    PFile *pf, *tmp;

    if (!filerepo)
        return;

    HASH_ITER(hh, filerepo->files, pf, tmp) {
        HASH_DEL(filerepo->files, pf);
        PFile_del(pf);
    }

    free(filerepo);
}
