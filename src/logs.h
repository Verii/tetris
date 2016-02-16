/*
 * Copyright (C) 2014  James Smith <james@apertum.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <stdio.h>
#include <sys/queue.h>

#ifdef DEBUG
#define debug(M, ...) logs_to_file("[DEBUG] " M, ##__VA_ARGS__)
#else
#define debug(M, ...)
#endif

#define log_err(M, ...)                                                        \
  do {                                                                         \
    logs_to_file("[ERR] " M " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__);    \
    fflush(stderr);                                                            \
  } while (0)

#define log_warn(M, ...)                                                       \
  logs_to_file("[WARN] " M " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

#define log_info(M, ...)                                                       \
  logs_to_file("[INFO] " M " (%s:%d)", ##__VA_ARGS__, __FILE__, __LINE__)

LIST_HEAD(log_entry_head, log_entry) entry_head;
struct log_entry {
  char *msg;
  LIST_ENTRY(log_entry) entries;
};

int logs_init(const char *path);
void logs_cleanup(void);

void logs_to_game(const char *, ...);
void logs_to_file(const char *, ...);
