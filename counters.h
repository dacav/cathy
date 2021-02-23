#pragma once

#include <stddef.h>

typedef struct Counters
{
    size_t freed_space;
    unsigned removed_files;
} Counters;

void Counters_print(const Counters *counters);
