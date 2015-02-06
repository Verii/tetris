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

#include "tetris.h"
#include <signal.h>
#include <stdlib.h>

typedef struct events events;

typedef int (*events_callback)(events *);

/**
 * events structure contains the file descriptor to listen on,
 * and the callback function to call when input is detected on the fd.
 */
struct events {
	int fd;
	events_callback cb;
};

/**
 * Listen on File Descriptor fd for IO, then call cb.
 */
int events_add_input(int fd, events_callback cb);
int events_remove_IO(int fd);

/**
 * Create a POSIX timer to signal the program (sig) periodically (timespec).
 * The raised signal is caught by the function defined the (sigaction).
 */
int events_add_timer(struct timespec, struct sigaction, int sig);

/**
 * Main loop of the program, pselect() for interrupts and keyboard/network
 * input, handling game commands, etc.
 *
 * Returns when the user quits, the server tells us to, or the game is over.
 */
void events_main_loop(tetris *);

void events_cleanup(void);

#endif /* EVENTS_H_ */
