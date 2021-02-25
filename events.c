#include "events.h"

#include <stdlib.h>
#include <err.h>

#include "events.h"

struct Events {
    struct {
        size_t total_space;
        size_t freed_space;
        unsigned unique_files;
        unsigned removed_files;
        unsigned ignored_links;
        unsigned collisions;
        unsigned bad_timestamps;
    } counters;
};

void Events_accept_file(Events *events, const File *file)
{
    events->counters.unique_files++;
    events->counters.total_space += file->size;
}

void Events_reject_file(Events *events, const File *file)
{
    events->counters.removed_files++;
    events->counters.freed_space += file->size;
}

void Events_duplicate(Events *events, const File *kept, const File *dropped)
{
    if (kept->mtime != dropped->mtime)
        events->counters.bad_timestamps++;
}

void Events_ignored_identical(Events *events, const File *kept, const File *dropped)
{
    (void)kept;
    (void)dropped;
    events->counters.ignored_links++;
}

void Events_collision(Events *events)
{
    events->counters.collisions++;
}

#define print(events, field, fmt) \
    warnx("  %-15s: " fmt, #field, (events)->counters.field);
void Events_print_stats(const Events *events, bool dry_run)
{
    warnx("Totals%s:", dry_run ? " (dry run)" : "");
    print(events, total_space, "%zu bytes");
    print(events, freed_space, "%zu bytes");
    print(events, unique_files, "%u");
    print(events, removed_files, "%u");
    print(events, ignored_links, "%u");
    print(events, collisions, "%u");
    print(events, bad_timestamps, "%u");
}
#undef print

void Events_del(Events *events)
{
    free(events);
}

Events *Events_new(void)
{
    Events *events;

    events = malloc(sizeof(Events));
    if (!events) {
        warn("malloc");
        return NULL;
    }

    *events = (Events){};
    return events;
}
