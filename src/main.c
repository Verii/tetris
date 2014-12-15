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
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "conf.h"
#include "logs.h"
#include "blocks.h"
#include "screen.h"
#include "events.h"

struct blocks_game *pgame;

extern void timer_handler(int);
extern int input_handler(union events_value);

/* We can exit() at any point and still safely cleanup */
static void cleanup(void)
{
	screen_cleanup();
//	network_cleanup();
	db_cleanup();
	blocks_cleanup(pgame);
	logs_cleanup();
	conf_cleanup();

	printf("Thanks for playing Tetris-%s!\n", VERSION);
}

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
	"Defaults:\n\t"
	"Hostname: \"%s\"\tPort: \"%s\"\n\t"
	"Log directory: \"%s\"\n\t"
	"Save directory: \"%s\"\n\t"
	"Configuration directory: \"%s\"\n\n"
	"Usage:\n\t"
	"[-u] usage\n\t"
	"[-c file] path to use for configuration file\n\t"
	"[-s file] location to save database\n\t"
	"[-l file] location to write logs\n\t"
	"[-h host] hostname to connect to\n\t"
	"[-p port] port to connect to\n";

	fprintf(stderr, help, __progname, VERSION, __DATE__, __TIME__,
		CONF_HOSTNAME, CONF_PORT, CONF_LOGS, CONF_SAVES, CONF_CONFIG);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	srand(time(NULL));

	bool cflag, hflag, lflag, pflag, sflag;
	cflag = hflag = lflag = pflag = sflag = false;

	char conffile[256], hostname[256], port[16];
	char logfile[256], savefile[256];

	/* Quit if we're not attached to a tty */
	if (!isatty(fileno(stdin)))
		exit(EXIT_FAILURE);

	int ch;
	while ((ch = getopt(argc, argv, "c:h:l:p:s:u")) != -1) {
		switch(ch) {
		case 'c':
			/* update location for configuration file */
			cflag = true;
			strncpy(conffile, optarg, 256);
			conffile[sizeof conffile -1] = '\0';
			break;
		case 'h':
			/* change default host name */
			hflag = true;
			strncpy(hostname, optarg, 128);
			hostname[sizeof hostname -1] = '\0';
			break;
		case 'l':
			/* logfile location */
			lflag = true;
			strncpy(logfile, optarg, 256);
			logfile[sizeof logfile -1] = '\0';
			break;
		case 'p':
			/* change default port number */
			pflag = true;
			strncpy(port, optarg, 16);
			logfile[sizeof port -1] = '\0';
			break;
		case 's':
			/* db location */
			sflag = true;
			strncpy(savefile, optarg, 256);
			logfile[sizeof savefile -1] = '\0';
			break;
		case 'u':
		default:
			usage();
			break;
		}
	}

	/* This function creates and defines global variables accessed by each
	 * of the subsystem init() functions. Unless non-NULL is passed to an
	 * init function, it will proceed with either the default, compiled-in
	 * value, or use the global value as read in from the configuration
	 * file.
	 *
	 * If a valid string is passed to one of the init subsystems, then that
	 * string takes precedence, even over the configuration file.
	 *
	 * So if we start the game as `blocks -h example.com -p 100`, and the
	 * configuration file specifies singleplayer, we will still try to play
	 * multiplayer.
	 */
	if (conf_init(cflag? conffile: NULL) != 1)
		exit(EXIT_FAILURE);

	if ((logs_init(lflag? logfile: NULL) != 1) ||
	    (blocks_init(&pgame) != 1) ||
	    (db_init(sflag? savefile: NULL) != 1) ||
//	    (network_init(hflag? hostname: NULL, pflag? port: NULL) != 1) ||
	    (screen_init() != 1))
		exit(EXIT_FAILURE);

	db_resume_state(pgame);

	atexit(cleanup);

	blocks_update_ghost_block(pgame, pgame->ghost);
	screen_draw_game(pgame);

	struct timespec ts_tick;
	ts_tick.tv_sec = 1;
	ts_tick.tv_nsec = 0;

	struct sigaction sa_tick;
	sa_tick.sa_flags = 0;
	sa_tick.sa_handler = timer_handler;
	sigemptyset(&sa_tick.sa_mask);

	/* Create timer */
	events_add_timer_event(ts_tick, sa_tick, SIGRTMIN+2);

	/* Add events to event loop */
	events_add_input_event(fileno(stdin), input_handler);

	events_main_loop();

	if (pgame->lose) {
		db_save_score(pgame);
		screen_draw_over();
	} else {
		db_save_state(pgame);
	}

	return 0;
}
