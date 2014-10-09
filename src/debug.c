/*
 * Copyright (C) 2014  James Smith <james@theta.pw>
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

/* Prints a log message of the form:
 * "[time] message"
 */
void debug_log(const char *fmt, ...)
{
	time_t s = time(NULL);
	char *msg, date[32];
	va_list ap;
	int ret;

	strftime(date, sizeof date, "[%F %H:%M]", localtime(&s));

	va_start(ap, fmt);
	ret = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		msg = NULL;
	} else {
		int len = strlen(msg);
		if (msg[len] == '\n')
			msg[len] = '\0';
	}

	fprintf(stderr, "%s %s\n", date, msg ? msg : "");
	free(msg);
}
