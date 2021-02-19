#pragma once

#include <stddef.h>

typedef struct OutDir OutDir;

OutDir *OutDir_new(const char *path);
int OutDir_link(const OutDir *outdir, const char *hash, const char *path);
void OutDir_del(OutDir *outdir);
