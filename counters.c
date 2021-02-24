#include "counters.h"

#include <err.h>

#define print(counters, field, fmt) \
    warnx("  " #field ": " fmt, (counters)->field);

void Counters_print(const Counters *counters, bool dry_run)
{
    warnx("Totals%s:", dry_run ? "(dry run)" : "");
    print(counters, removed_files, "%u");
    print(counters, freed_space, "%zu bytes");
    print(counters, ignored_links, "%u");
    print(counters, collisions, "%u");
}
