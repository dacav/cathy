#pragma once

#include "file.h"

typedef struct Events Events;

Events *Events_new(void);

void Events_accept_file(Events *, const File *);
void Events_reject_file(Events *, const File *);
void Events_collision(Events *);
void Events_duplicate(Events *, const File *, const File *);
void Events_ignored_identical(Events *, const File *, const File *);

void Events_print_stats(const Events *, bool dry_run);

void Events_del(Events *);
