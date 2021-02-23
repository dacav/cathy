#include "counters.h"

#include <stdio.h>

#define print(counters, field, fmt) \
    fprintf(stderr, #field ": " fmt "\n", (counters)->field);

void Counters_print(const Counters *counters)
{
    print(counters, removed_files, "%u");
    print(counters, freed_space, "%zu bytes");
}
