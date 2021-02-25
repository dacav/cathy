#include "events.h"

#include <stdlib.h>
#include <err.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h>

#include "events.h"
#include "util.h"

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

    FILE *logfile;
};

static
void say(const Events *events, const char *fmt, ...)
{
    va_list ap;

    if (!events->logfile)
        return;

    va_start(ap, fmt);
    vfprintf(events->logfile, fmt, ap);
    va_end(ap);
}

void Events_accept_file(Events *events, const File *file)
{
    say(events, "Accept: " File_FMT "\n", File_REPR(file));
    events->counters.unique_files++;
    events->counters.total_space += file->size;
}

void Events_reject_file(Events *events, const File *file)
{
    say(events, "Reject: " File_FMT "\n", File_REPR(file));
    events->counters.removed_files++;
    events->counters.freed_space += file->size;
}

void Events_duplicate(Events *events, const File *kept, const File *dropped)
{
    bool bad_timestamp;

    bad_timestamp = kept->mtime != dropped->mtime;

    say(events, "Duplicate: " File_FMT " replaces " File_FMT "%s\n",
        File_REPR(kept), File_REPR(dropped),
        bad_timestamp ? " (bad timestamp)" : "");

    if (bad_timestamp)
        events->counters.bad_timestamps++;
}

void Events_ignored_identical(Events *events, const File *kept, const File *dropped)
{
    say(events, "Ignore: " File_FMT " is the same as " File_FMT "\n",
        File_REPR(kept), File_REPR(dropped));
    events->counters.ignored_links++;
}

void Events_skipped_filename(Events *events, const char *fname)
{
    say(events, "Skpped: '%s'\n", fname);
    events->counters.skipped++;
}

void Events_collision(Events *events, const File *file, const char *hash)
{
    say(events, "Collision: " File_FMT " having hash '%s'\n",
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
    if (!events)
        return;

    if (events->logfile)
        fclose(events->logfile);
    free(events);
}

Events *Events_new(const char *events_logfile)
{
    Events *events = NULL;
    int logfd_dup = -1;

    events = malloc(sizeof(Events));
    if (!events) {
        warn("malloc");
        goto fail;
    }

    *events = (Events){};

    if (events_logfile) {
        events->logfile = fopen(events_logfile, "w");
        if (!events->logfile) {
            warn("fopen(%s, ...)", events_logfile);
            goto fail;
        }
    }

    return events;

fail:
    Events_del(events);
    Util_fdclose(&logfd_dup);
    return NULL;
}
