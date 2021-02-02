#pragma once

#include "file.h"

typedef struct Events Events;

Events *Events_new(const char *logfile);

void Events_accept_file(Events *, const File *);
void Events_reject_file(Events *, const File *);
void Events_collision(Events *, const File *, const char *hash);
void Events_duplicate(Events *, const File *, const File *);
void Events_ignored_identical(Events *, const File *, const File *);
void Events_skipped_filename(Events *, const char *fname);

void Events_print_stats(const Events *, bool dry_run);

void Events_del(Events *);
