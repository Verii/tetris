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

#include <stdlib.h>
#include <string.h>
#include <sys/queue.h>

#include "log_queue.h"

int log_queue_add_entry(const char *msg)
{
	int msg_len = -1;
	struct log_entry *np = calloc(1, sizeof *np);
	if (!np)
		return -1;

	msg_len = strlen(msg)+1;

	np->msg = malloc(msg_len);
	if (!np->msg) {
		free(np);
		return -1;
	}

	strncpy(np->msg, msg, msg_len);

	LIST_INSERT_HEAD(&entry_head, np, entries);

	return msg_len;
}
