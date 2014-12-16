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

#ifndef EVENTS_H_
#define EVENTS_H_

#include <signal.h>
#include "blocks.h"

union events_value {
	void *val_ptr;
	int val_int;
};

typedef int (*events_callback)(union events_value);

/* TODO */
int events_add_input_event(int fd, events_callback cb);
int events_add_output_event(int fd, events_callback cb);

int events_add_timer_event(struct timespec, struct sigaction, int sig);
void events_main_loop(void);
void events_cleanup(void);

/* Right, so admittedly this is pretty sloppy. There are global defines
 * everywhere and the logical flow is just all over the place.
 *
 * TODO make this pretty.
 */
void timer_handler(int);
int input_handler(union events_value ev);

#endif /* EVENTS_H_ */
