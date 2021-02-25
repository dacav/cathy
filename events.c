#include "events.h"

#include <stdlib.h>
#include <err.h>
#include <stdarg.h>

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
        unsigned skipped;
    } counters;

    bool verbose;
};

static
void say(const Events *events, const char *fmt, ...)
{
    va_list ap;

    if (!events->verbose)
        return;

    va_start(ap, fmt);
    vwarnx(fmt, ap);
    va_end(ap);
}

void Events_accept_file(Events *events, const File *file)
{
    say(events, "accept: " File_FMT, File_REPR(file));
    events->counters.unique_files++;
    events->counters.total_space += file->size;
}

void Events_reject_file(Events *events, const File *file)
{
    say(events, "reject: " File_FMT, File_REPR(file));
    events->counters.removed_files++;
    events->counters.freed_space += file->size;
}

void Events_duplicate(Events *events, const File *kept, const File *dropped)
{
    say(events, "duplicate: " File_FMT " replaces " File_FMT,
        File_REPR(kept), File_REPR(dropped));
    if (kept->mtime != dropped->mtime)
        events->counters.bad_timestamps++;
}

void Events_ignored_identical(Events *events, const File *kept, const File *dropped)
{
    say(events, "ignore: " File_FMT " is the same as " File_FMT,
        File_REPR(kept), File_REPR(dropped));
    events->counters.ignored_links++;
}

void Events_skipped_filename(Events *events, const char *fname)
{
    say(events, "skpped: '%s'", fname);
    events->counters.skipped++;
}

void Events_collision(Events *events, const File *file, const char *hash)
{
    say(events, "collision: " File_FMT " having hash '%s'",
        File_REPR(file), hash);
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
    print(events, skipped, "%u");
}
#undef print

void Events_del(Events *events)
{
    free(events);
}

Events *Events_new(bool verbose)
{
    Events *events;

    events = malloc(sizeof(Events));
    if (!events) {
        warn("malloc");
        return NULL;
    }

    *events = (Events){
        .verbose = verbose,
    };
    return events;
}
