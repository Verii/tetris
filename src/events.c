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
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#include "events.h"
#include "screen.h"
#include "tetris.h"
#include "logs.h"

/* Set by signal handler */
extern volatile sig_atomic_t tetris_do_tick;

/* For POSIX timer, ignore mask blocks the generated signal from everyone but
 * pselect().
 */
static timer_t timerid;
static sigset_t ignore_mask;

/* pselect() read/write fd sets */
static fd_set master_read;
static fd_set master_write;
static uint8_t fd_max;

#define NUM_EVENTS 2
static events *p_events[NUM_EVENTS];

static int events_reset_timer(timer_t td, struct timespec ts) {
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

int events_add_timer(struct timespec ts, struct sigaction sa, int sig) {
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

int events_add_input(int fd, events_callback cb) {
  if (fd < 0)
    return 0;

  events *new_event = malloc(sizeof *new_event);
  if (!new_event) {
    log_err("Out of memory");
    return -1;
  }

  new_event->fd = fd;
  new_event->cb = cb;

  size_t i = 0;
  while (i < NUM_EVENTS && p_events[i])
    i++;

  if (i >= NUM_EVENTS) {
    free(new_event);
    return -1;
  } else {
    p_events[i] = new_event;
  }

  FD_SET(fd, &master_read);

  if (fd > fd_max)
    fd_max = fd;

  debug("Registered event: %d/%d from fd %d", i, NUM_EVENTS - 1,
        p_events[i]->fd);

  return 1;
}

int events_remove_IO(int fd) {
  if (fd < 0)
    return 0;

  size_t i;
  for (i = 0; i < NUM_EVENTS; i++) {
    if (p_events[i] && p_events[i]->fd == fd)
      break;
  }

  if (i == NUM_EVENTS)
    return 0;

  close(p_events[i]->fd);

  if (FD_ISSET(p_events[i]->fd, &master_read))
    FD_CLR(p_events[i]->fd, &master_read);

  if (FD_ISSET(p_events[i]->fd, &master_write))
    FD_CLR(p_events[i]->fd, &master_write);

  free(p_events[i]);
  p_events[i] = NULL;

  debug("Removed event: %d/%d from fd %d", i, NUM_EVENTS - 1, fd);

  fd_max = 0;
  for (i = 0; i < NUM_EVENTS; i++) {
    if (p_events[i] && p_events[i]->fd > fd_max)
      fd_max = p_events[i]->fd;
  }

  return 1;
}

/* Game is over when this function returns
 * We also handle interrupts in the form of signals from POSIX timers.
 */
void events_main_loop(tetris *pgame) {
  while (1) {

    fd_set read_fds = master_read;
    fd_set write_fds = master_write;

    sigset_t empty_mask;
    sigemptyset(&empty_mask);

    struct timespec ps_timeout = {
        .tv_nsec = 0, .tv_sec = 2,
    };

    errno = 0;
    int ps_ret = pselect(fd_max + 1, &read_fds, &write_fds, NULL, &ps_timeout,
                         &empty_mask);

    if (ps_ret == -1 && errno != EINTR) {
      perror("pselect()");
      continue;
    }

    if (tetris_do_tick > 0) {
      tetris_do_tick = 0;

      if (tetris_cmd(pgame, TETRIS_GAME_TICK) < 0)
        return;

      screen_update(pgame);

      /* Update tick timer */
      struct timespec ts;
      ts.tv_sec = 0;
      ts.tv_nsec = tetris_get_delay(pgame);

      events_reset_timer(timerid, ts);
    }

    if (ps_ret <= 0)
      continue;

    for (size_t i = 0; i < NUM_EVENTS; i++) {
      if (!p_events[i])
        continue;

      if (!(FD_ISSET(p_events[i]->fd, &read_fds) ||
            FD_ISSET(p_events[i]->fd, &write_fds)))
        continue;

      /* Call registered callback, return if the callback fails */
      if (p_events[i]->cb(p_events[i]) < 0)
        return;
    }

  } /* while */
}

static void events_timer_cleanup(void) {
  if (timerid != 0)
    timer_delete(timerid);
  timerid = 0;
}

static void events_IO_cleanup(void) {
  size_t i;
  for (i = 0; i < NUM_EVENTS; i++)
    if (p_events[i])
      events_remove_IO(p_events[i]->fd);
}

void events_cleanup(void) {
  events_timer_cleanup();
  events_IO_cleanup();
}
