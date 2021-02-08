#include <stddef.h>

typedef struct {
    int hashdir;
    size_t hashlen;
} OutDir;

int OutDir_init(OutDir *outdir, const char *path);
int OutDir_link(const OutDir *outdir, const char *hash, const char *path);
void OutDir_free(OutDir *outdir);
