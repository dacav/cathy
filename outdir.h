#pragma once

#include <stddef.h>

typedef struct OutDir OutDir;

typedef struct {
    const char *hash;
    const char *path;
} OutDir_LinkInfo;

OutDir *OutDir_new(const char *path);
int OutDir_link(const OutDir *outdir, const OutDir_LinkInfo *);
void OutDir_del(OutDir *outdir);
