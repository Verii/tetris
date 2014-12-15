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

#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "screen.h"
#include "blocks.h"
#include "helpers.h"
#include "logs.h"
#include "events.h"

static sigset_t ignore_mask;
static fd_set master_read;
static uint8_t fd_max;

static struct events {
	int fd;
	events_callback cb;
	union events_value val;
} events[4];

void timer_handler(int sig)
{
	blocks_do_update = sig;
}

int input_handler(union events_value ev)
{
	int ret;

	debug("Input Handler");

	extern struct blocks_game *pgame;
	ret = blocks_input(pgame, ev.val_int);
	screen_draw_game(pgame);
}

void events_main_loop(void)
{
	int ret, num_events, success;
	fd_set read_fds;

	sigset_t empty_mask;
	sigemptyset(&empty_mask);

	while (1) {
		num_events = 0;
		success = 0;

		read_fds = master_read;

		ret = pselect(fd_max+1, &read_fds, NULL, NULL,
				NULL, &empty_mask);

		if (ret == -1 && errno != EINTR) {
			perror("pselect()");
			continue;
		} else if (ret == 0) {
			continue;
		}

		if (blocks_do_update > 0) {
			blocks_do_update = 0;

			extern struct blocks_game *pgame;
			blocks_tick(pgame);
			screen_draw_game(pgame);
		}

		for (size_t i = 0; i < LEN(events); i++) {
			if (num_events >= ret || events[i].cb == NULL)
				break;

			if (!FD_ISSET(i, &read_fds))
				continue;

			num_events++;

			char buf;
			read(events[i].fd, &buf, 1);
			events[i].val.val_int = buf;

			success = events[i].cb(events[i].val);
		}

		if (success < 0)
			goto completed;
	}

completed:
	return;
}

int events_add_timer_event(struct timespec ts, struct sigaction sa, int sig)
{
	sigaddset(&ignore_mask, sig);
	sigprocmask(SIG_BLOCK, &ignore_mask, NULL);

	sigaction(sig, &sa, NULL);

	timer_t timerid;
	struct sigevent evp;
	memset(&evp, 0, sizeof evp);
	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = sig;

	if (timer_create(CLOCK_MONOTONIC, &evp, &timerid) != 0) {
		perror("timer_create()");
		return -1;
	}

	struct itimerspec timer_ts;
	memset(&timer_ts, 0, sizeof timer_ts);
	timer_ts.it_interval.tv_sec = ts.tv_sec;
	timer_ts.it_interval.tv_nsec = ts.tv_nsec;
	timer_ts.it_value.tv_sec = 1;

	if (timer_settime(timerid, 0, &timer_ts, NULL) != 0) {
		perror("timer_settime()");
		return -1;
	}

	return 1;
}

int events_add_input_event(int fd, events_callback cb)
{
	size_t i;

	for (i = 0; i < LEN(events); i++) {
		if (events[i].cb)
			continue;

		events[i].fd = fd;
		events[i].cb = cb;
		break;
	}

	if (i >= LEN(events))
		return -1;

	FD_SET(fd, &master_read);

	if (fd > fd_max)
		fd_max = fd;

	debug("Registered event: %d/%d from fd %d",
			i, LEN(events), events[i].fd);

	return 1;
}
