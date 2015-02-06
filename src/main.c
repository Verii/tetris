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
#include "net/network.h"

tetris *pgame;
struct config *config;
volatile sig_atomic_t tetris_do_tick;

static void usage(void)
{
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
	"[-l file] location to write logs\n\t"
	"[-h host] hostname to connect to\n\t"
	"[-p port] port to connect to\n";

	fprintf(stderr, help, __progname, VERSION, __DATE__, __TIME__);
}

void timer_handler(int sig)
{
	tetris_do_tick = sig;
}

int main(int argc, char **argv)
{
	bool cflag, hflag, lflag, pflag, sflag;
	char conffile[256], hostname[256], port[16];
	char logfile[256], savefile[256];
	int ch;

	setlocale(LC_ALL, "");

	/* Quit if we're not attached to a tty */
	if (!isatty(fileno(stdin)))
		exit(EXIT_FAILURE);

	cflag = hflag = lflag = pflag = sflag = false;
	while ((ch = getopt(argc, argv, "c:h:l:p:s:u")) != -1) {
		switch(ch) {
		case 'c':
			/* update location for configuration file */
			cflag = true;
			strncpy(conffile, optarg, sizeof conffile);
			conffile[sizeof conffile -1] = '\0';
			break;
		case 'h':
			/* change default host name */
			hflag = true;
			strncpy(hostname, optarg, sizeof hostname);
			hostname[sizeof hostname -1] = '\0';
			break;
		case 'l':
			/* logfile location */
			lflag = true;
			strncpy(logfile, optarg, sizeof logfile);
			logfile[sizeof logfile -1] = '\0';
			break;
		case 'p':
			/* change default port number */
			pflag = true;
			strncpy(port, optarg, sizeof port);
			port[sizeof port -1] = '\0';
			break;
		case 's':
			/* db location */
			sflag = true;
			strncpy(savefile, optarg, sizeof savefile);
			savefile[sizeof savefile -1] = '\0';
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
	if (conf_init(config, cflag? conffile: config->_conf_file.val) != 1)
		exit(EXIT_FAILURE);

	if (logs_init(lflag? logfile: config->logs_file.val) != 1)
		exit(EXIT_FAILURE);

	if (tetris_init(&pgame) != 1 || pgame == NULL)
		exit(EXIT_FAILURE);

	tetris_set_name(pgame, config->username.val);
	tetris_set_dbfile(pgame, config->save_file.val);

	tetris_set_ghosts(pgame, TETRIS_TRUE);
	tetris_set_wallkicks(pgame, TETRIS_TRUE);
	tetris_set_tspins(pgame, TETRIS_TRUE);
	tetris_set_lockdelay(pgame, TETRIS_TRUE);

	tetris_set_win_condition(pgame, TETRIS_40_LINES);
//	tetris_set_win_condition(pgame, TETRIS_CLASSIC);

	db_resume_state(pgame);

	/* Draw with Ncurses context by default */
#if !defined(DEBUG)
	screen_init = &screen_nc_init;
	screen_cleanup = &screen_nc_cleanup;
	screen_menu = &screen_nc_menu;
	screen_update = &screen_nc_update;
	screen_gameover = &screen_nc_gameover;
#else
	/* Because valgrind output draws to ncurses screen, it's a pain in the
	 * ass to read. So redefine draw functions to dummy void functions when
	 * we're trying to debug the program.
	 */
	screen_init = &screen_nodraw_init;
	screen_cleanup = &screen_nodraw_cleanup;
	screen_menu = &screen_nodraw_menu;
	screen_update = &screen_nodraw_update;
	screen_gameover = &screen_nodraw_gameover;
#endif

	/* Create ncurses context, draw screen, and watch for keyboard input */
	screen_init();
	screen_menu(pgame);
	screen_update(pgame);

	events_add_input(fileno(stdin), keyboard_in_handler);

	/* Add network FD to events watch list */
	int server_fd = network_init(hflag? hostname: config->hostname.val,
			pflag? port: config->port.val);

	events_add_input(server_fd, network_in_handler);

	struct timespec ts_tick;
	ts_tick.tv_sec = 0;
	ts_tick.tv_nsec = tetris_get_delay(pgame);

	struct sigaction sa_tick;
	sa_tick.sa_flags = 0;
	sa_tick.sa_handler = timer_handler;
	sigemptyset(&sa_tick.sa_mask);

	events_add_timer(ts_tick, sa_tick, SIGRTMIN+2);

	/* Main loop of program */
	events_main_loop(pgame);

	switch (tetris_get_state(pgame)) {
	case TETRIS_LOSE:
	case TETRIS_WIN:
		db_save_score(pgame);
		break;
	case TETRIS_QUIT:
		db_save_state(pgame);
		break;
	}

	/* Cleanup */
	conf_cleanup(config);
	screen_gameover(pgame);
	screen_cleanup();

	tetris_cleanup(pgame);
	logs_cleanup();
	network_cleanup();
	events_cleanup();

	return 0;
}
