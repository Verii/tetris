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

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>

#include "db.h"
#include "conf.h"
#include "logs.h"
#include "tetris.h"
#include "screen.h"
#include "input.h"
#include "events.h"

tetris *pgame;
struct config *config;
volatile sig_atomic_t tetris_do_tick;

static void usage(void) {
  extern const char *__progname;
  const char *help =
      "Copyright (C) 2014 James Smith <james@apertum.org>\n\n"
      "This program is free software; you can redistribute it and/or modify\n"
      "it under the terms of the GNU General Public License as published by\n"
      "the Free Software Foundation; either version 2 of the License, or\n"
      "(at your option) any later version.\n\n"
      "This program is distributed in the hope that it will be useful,\n"
      "but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
      "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
      "GNU General Public License for more details.\n\n"
      "%s version %s\n"
      "Built on %s at %s\n\n"
      "Usage:\n\t"
      "[-u] usage\n\t"
      "[-c file] path to use for configuration file\n\t"
      "[-s file] location to save database\n\t"
      "[-l file] location to write logs\n\n";

  fprintf(stderr, help, __progname, VERSION, __DATE__, __TIME__);
}

void timer_handler(int sig) { tetris_do_tick = sig; }

int main(int argc, char **argv) {
  bool cflag, hflag, lflag, pflag, sflag;
  char conffile[256];
  char logfile[256], savefile[256];
  int ch;

  setlocale(LC_ALL, "");

  /* Quit if we're not attached to a tty */
  if (!isatty(fileno(stdin)))
    exit(EXIT_FAILURE);

  cflag = hflag = lflag = pflag = sflag = false;
  while ((ch = getopt(argc, argv, "c:h:l:p:s:u")) != -1) {
    switch (ch) {
    case 'c':
      /* update location for configuration file */
      cflag = true;
      strncpy(conffile, optarg, sizeof conffile);
      conffile[sizeof conffile - 1] = '\0';
      break;
    case 'l':
      /* logfile location */
      lflag = true;
      strncpy(logfile, optarg, sizeof logfile);
      logfile[sizeof logfile - 1] = '\0';
      break;
    case 's':
      /* db location */
      sflag = true;
      strncpy(savefile, optarg, sizeof savefile);
      savefile[sizeof savefile - 1] = '\0';
      break;
    case 'u':
    default:
      usage();
      exit(EXIT_FAILURE);
      break;
    }
  }

  /* Create globals from built in config */
  if (conf_create(&config) != 1)
    exit(EXIT_FAILURE);

  /* Apply built-in configuration options */
  if (conf_init(config, NULL) != 1)
    exit(EXIT_FAILURE);

  /* Try to read default configuration file, or user supplied
   * configuration file
   */
  if (conf_init(config, cflag ? conffile : config->_conf_file.val) != 1)
    exit(EXIT_FAILURE);

  if (logs_init(lflag ? logfile : config->logs_file.val) != 1)
    exit(EXIT_FAILURE);

  if (tetris_init(&pgame) != 1 || pgame == NULL)
    exit(EXIT_FAILURE);

  tetris_set_name(pgame, config->username.val);

#ifdef DEBUG
  /* Use a memory db for debugging */
  tetris_set_dbfile(pgame, ":memory:");
#else
  tetris_set_dbfile(pgame, config->save_file.val);
#endif

  /* Newer-ish version of tetris with wallkicks, ghost blocks, lock
   * delays, tspins. Infinity edition!
   */
  tetris_set_gamemode(pgame, TETRIS_INFINITY);

  /* Classic tetris, nothing fancy. Play until you lose. */
  //	tetris_set_gamemode(pgame, TETRIS_CLASSIC);

  if (db_resume_state(pgame) != 1)
    logs_to_game("Unable to resume old game save.");

  /* Create ncurses context, draw screen, and watch for keyboard input */
  screen_init();
  screen_menu(pgame);
  screen_update(pgame);

  events_add_input(fileno(stdin), keyboard_in_handler);

  struct timespec ts_tick;
  ts_tick.tv_sec = 0;
  ts_tick.tv_nsec = tetris_get_delay(pgame);

  struct sigaction sa_tick;
  sa_tick.sa_flags = 0;
  sa_tick.sa_handler = timer_handler;
  sigemptyset(&sa_tick.sa_mask);

  /* Add timer event to trigger game ticks */
  events_add_timer(ts_tick, sa_tick, SIGRTMIN);

  /* Main loop of program */
  events_main_loop(pgame);

  switch (tetris_get_state(pgame)) {
  case TETRIS_LOSE:
  case TETRIS_WIN:
    db_save_score(pgame);
    break;
  default:
  case TETRIS_QUIT:
    db_save_state(pgame);
    break;
  }

  /* Cleanup */
  screen_gameover(pgame);
  screen_cleanup();
  tetris_cleanup(pgame);
  events_cleanup();

  conf_cleanup(config);
  logs_cleanup();

  return 0;
}
