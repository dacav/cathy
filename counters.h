#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct Counters
{
    size_t freed_space;
    unsigned removed_files;
    unsigned ignored_links;
} Counters;

void Counters_print(const Counters *counters, bool dry_run);
