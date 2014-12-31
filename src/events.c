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
#include <sys/select.h>
#include <time.h>
#include <signal.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "events.h"
#include "conf.h"
#include "screen.h"
#include "tetris.h"
#include "helpers.h"
#include "logs.h"

/* Set by signal handler */
volatile sig_atomic_t tetris_do_tick;
extern tetris *pgame;
static timer_t timerid;
static sigset_t ignore_mask;
static fd_set master_read;
static uint8_t fd_max;

static struct events {
	int fd;
	events_callback cb;
	union events_value val;
} events[2];

void timer_handler(int sig)
{
	tetris_do_tick = sig;
}

int input_handler(union events_value ev)
{
	int ret, cmd = -1;
	size_t i;
	struct config *conf = conf_get_globals_s();

	struct {
		struct key_bindings *key;
		int cmd;
	} actions[] = {
		{ &conf->move_drop, TETRIS_MOVE_DROP },
		{ &conf->move_down, TETRIS_MOVE_DOWN },
		{ &conf->move_left, TETRIS_MOVE_LEFT },
		{ &conf->move_right, TETRIS_MOVE_RIGHT },
		{ &conf->rotate_left, TETRIS_ROT_LEFT },
		{ &conf->rotate_right, TETRIS_ROT_RIGHT },

		{ &conf->hold_key, TETRIS_HOLD_BLOCK },
		{ &conf->quit_key, TETRIS_QUIT_GAME },
		{ &conf->pause_key, TETRIS_PAUSE_GAME },
	};

	for (i = 0; i < LEN(actions); i++) {
		if (actions[i].key->key != ev.val_int)
			continue;

		/* Don't accept disabled keys. */
		if (!actions[i].key->enabled)
			continue;

		cmd = actions[i].cmd;
		break;
	}

	if (i >= LEN(actions) || cmd == -1)
		return 0;

	ret = tetris_cmd(pgame, cmd);
	screen_draw_game(pgame);

	return ret;
}

static int events_reset_timer(timer_t td, struct timespec ts)
{
	struct itimerspec timer_ts;
	timer_ts.it_interval.tv_sec = ts.tv_sec;
	timer_ts.it_interval.tv_nsec = ts.tv_nsec;
	timer_ts.it_value.tv_sec = ts.tv_sec;
	timer_ts.it_value.tv_nsec = ts.tv_nsec;

	if (timer_settime(td, 0, &timer_ts, NULL) != 0) {
		perror("timer_settime()");
		return -1;
	}

	return 1;
}

int events_add_timer_event(struct timespec ts, struct sigaction sa, int sig)
{
	sigaddset(&ignore_mask, sig);
	sigprocmask(SIG_BLOCK, &ignore_mask, NULL);

	sigaction(sig, &sa, NULL);

	struct sigevent evp;
	memset(&evp, 0, sizeof evp);
	evp.sigev_notify = SIGEV_SIGNAL;
	evp.sigev_signo = sig;

	if (timer_create(CLOCK_MONOTONIC, &evp, &timerid) != 0) {
		perror("timer_create()");
		return -1;
	}

	struct itimerspec timer_ts;
	timer_ts.it_interval.tv_sec = ts.tv_sec;
	timer_ts.it_interval.tv_nsec = ts.tv_nsec;
	timer_ts.it_value.tv_sec = ts.tv_sec;
	timer_ts.it_value.tv_nsec = ts.tv_nsec;

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

void events_cleanup(void)
{
	timer_delete(timerid);
}


/* Game is over when this function returns
 *
 * We wait for user input on stdin and the network socket(if available).
 * We also handle interrupts in the form of signals from POSIX timers.
 */
void events_main_loop(void)
{
	int ps_ret, num_events, success;
	fd_set read_fds;

	atexit(events_cleanup);

	sigset_t empty_mask;
	sigemptyset(&empty_mask);

	const struct timespec timeout = {
		.tv_nsec = 0,
		.tv_sec = 1,
	};

	while (1) {
		success = 0;
		num_events = 0;
		read_fds = master_read;

		errno = 0;
		ps_ret = pselect(fd_max+1, &read_fds, NULL, NULL,
				&timeout, &empty_mask);

		if (ps_ret == -1 && errno != EINTR) {
			perror("pselect()");
			continue;
		}

		if (tetris_do_tick > 0) {
			tetris_do_tick = 0;

			success = tetris_cmd(pgame, TETRIS_GAME_TICK);
			screen_draw_game(pgame);

			/* Update tick timer */
			int delay;
			tetris_get_attr(pgame, TETRIS_GET_DELAY, &delay);

			struct timespec ts;
			ts.tv_sec = 0;
			ts.tv_nsec = delay;

			events_reset_timer(timerid, ts);
		}

		for (size_t i = 0; ps_ret && i < LEN(events); i++) {
			if (num_events >= ps_ret || events[i].cb == NULL)
				break;

			if (!FD_ISSET(i, &read_fds))
				continue;

			num_events++;

			char buf;
			read(events[i].fd, &buf, 1);
			events[i].val.val_int = buf;

			success = events[i].cb(events[i].val);
		}

		/* Quit the game if one of the callbacks returns -1 */
		if (success < 0)
			break;
	}
}
