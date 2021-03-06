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

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>
#include <time.h>

#include "helpers.h"
#include "logs.h"

/* Internal function.
 * Wrapper, adds new message to head of linked list
 */
static int
_add_to_queue(const char* msg)
{
  int msg_len = -1;
  struct log_entry* np = calloc(1, sizeof *np);
  if (!np)
    return -1;

  msg_len = strlen(msg) + 1;

  np->msg = malloc(msg_len);
  if (!np->msg) {
    free(np);
    return -1;
  }

  strncpy(np->msg, msg, msg_len);

  LIST_INSERT_HEAD(&entry_head, np, entries);

  return msg_len;
}

/* Try to open file provided by user for writing. */
int
logs_init(const char* path)
{
  LIST_INIT(&entry_head);

  if (!path)
    return -1;

  fprintf(stderr, "Redirecting logs to %s\n", path);

  if (freopen(path, "a+", stderr) != NULL) {
    return 1;
  } else {
    log_warn("freopen: %s: %s", strerror(errno), path);
    return -1;
  }

  return 1;
}

/* Remove elements in linked list, close logfile. */
void
logs_cleanup(void)
{
  struct log_entry* np;

  while (entry_head.lh_first) {
    np = entry_head.lh_first;
    LIST_REMOVE(np, entries);
    free(np->msg);
    free(np);
  }

  /* Visual Game separator */
  fprintf(stderr, "--\n");
  fclose(stderr);
}

/* Adds log message to a message queue, to be printed in game */
void
logs_to_game(const char* fmt, ...)
{
  char* debug_message;
  va_list ap;

  va_start(ap, fmt);

  if (vasprintf(&debug_message, fmt, ap) < 0)
    debug_message = NULL;

  va_end(ap);

  if (debug_message)
    _add_to_queue(debug_message);

  free(debug_message);
}

/* Prints a log message of the form:
 * "[time] message"
 */
void
logs_to_file(const char* fmt, ...)
{
  va_list ap;
  char* debug_message;
  time_t s = time(NULL);
  char date[32];

  strftime(date, sizeof date, "[%F %H:%M]", localtime(&s));

  va_start(ap, fmt);

  if (vasprintf(&debug_message, fmt, ap) < 0)
    debug_message = NULL;

  va_end(ap);

  fprintf(stderr, "%s %s\n", date, debug_message ? debug_message : "");
  free(debug_message);
}
