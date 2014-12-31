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

#include "conf.h"
#include "logs.h"
#include "tetris.h"
#include "screen.h"
#include "events.h"
#include "network.h"

tetris *pgame;

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

int main(int argc, char **argv)
{
	bool cflag, hflag, lflag, pflag, sflag;
	char conffile[256], hostname[256], port[16];
	char logfile[256], savefile[256];
	int ch;

	setlocale(LC_ALL, "");
	srand(time(NULL));

	/* Quit if we're not attached to a tty */
	if (!isatty(fileno(stdin)))
		exit(EXIT_FAILURE);

	cflag = hflag = lflag = pflag = sflag = false;
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
			exit(EXIT_FAILURE);
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
	 * So if we start the game as `tetris -h example.com -p 100`, and the
	 * configuration file specifies singleplayer, we will still try to play
	 * multiplayer.
	 */
	if (conf_init(cflag? conffile: NULL) != 1)
		exit(EXIT_FAILURE);

	if ((logs_init(lflag? logfile: NULL) != 1) ||
	    (tetris_init(&pgame) != 1 || pgame == NULL) ||
	    (network_init(hflag? hostname: NULL, pflag? port: NULL) != 1) ||
	    (screen_init() != 1))
		exit(EXIT_FAILURE);

	screen_draw_game(pgame);

	int delay;
	tetris_get_attr(pgame, TETRIS_GET_DELAY, &delay);

	struct timespec ts_tick;
	ts_tick.tv_sec = 0;
	ts_tick.tv_nsec = delay;

	struct sigaction sa_tick;
	sa_tick.sa_flags = 0;
	sa_tick.sa_handler = timer_handler;
	sigemptyset(&sa_tick.sa_mask);

	/* Create timer */
	events_add_timer_event(ts_tick, sa_tick, SIGRTMIN+2);

	/* Add events to event loop */
	events_add_input_event(fileno(stdin), input_handler);

	events_main_loop();

	tetris_cleanup(pgame);

	return 0;
}
