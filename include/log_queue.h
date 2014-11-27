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

#ifndef LOG_QUEUE_H_
#define LOG_QUEUE_H_

#include <sys/queue.h>

LIST_HEAD(log_entry_head, log_entry) entry_head;
struct log_entry {
	char *msg;

	LIST_ENTRY(log_entry) entries;
};

int log_queue_add_entry(const char *);

/* Cleanup memory */
#define log_queue_cleanup() while (entry_head.lh_first) { \
		struct log_entry *tmp_ = entry_head.lh_first; \
		LIST_REMOVE(tmp_, entries); \
		free(tmp_->msg); \
		free(tmp_); \
	}

#endif /* LOG_QUEUE_H_ */
