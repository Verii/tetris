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

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"
#include "log_queue.h"

static void debug_format_message(char **strp, const char *fmt, va_list ap)
{
	int ret;
	ret = vasprintf(strp, fmt, ap);

	if (ret < 0) {
		*strp = NULL;
	} else {
		if ((*strp)[ret-1] == '\n')
			(*strp)[ret-1] = '\0';
	}
}

/* Adds log message to a message queue, to be printed in game.
 * Then prints the same message to stderr.
 */
void debug_ingame_log(const char *fmt, ...)
{
	char *debug_message;
	va_list ap;

	va_start(ap, fmt);
	debug_format_message(&debug_message, fmt, ap);
	va_end(ap);

	if (debug_message)
		log_queue_add_entry(debug_message);

	free(debug_message);
}

/* Prints a log message of the form:
 * "[time] message"
 */
void debug_log(const char *fmt, ...)
{
	char *debug_message;
	time_t s = time(NULL);
	char date[32];

	strftime(date, sizeof date, "[%F %H:%M]", localtime(&s));

	va_list ap;
	va_start(ap, fmt);
	debug_format_message(&debug_message, fmt, ap);
	va_end(ap);

	fprintf(stderr, "%s %s\n", date, debug_message ? debug_message : "");
	free(debug_message);
}
