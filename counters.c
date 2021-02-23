#include "counters.h"

#include <err.h>

#define print(counters, field, fmt) \
    warnx(#field ": " fmt "\n", (counters)->field);

void Counters_print(const Counters *counters)
{
    print(counters, removed_files, "%u");
    print(counters, freed_space, "%zu bytes");
}
