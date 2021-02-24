#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct Counters
{
    size_t total_space;
    size_t freed_space;
    unsigned unique_files;
    unsigned removed_files;
    unsigned ignored_links;
    unsigned collisions;
    unsigned bad_timestamps;
} Counters;

void Counters_print(const Counters *counters, bool dry_run);
