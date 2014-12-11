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

#include <ncurses.h>

#include "db.h"
#include "conf.h"
#include "logs.h"
#include "blocks.h"
#include "screen.h"

static struct blocks_game *pgame;

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

	/* Have the kernel create a timer for our process.
	 * This early version creates a static timer. If you're reading this
	 * then I'm currently working on lots of different things, but it will
	 * -- when finished -- be virtually indistinguishable from the
	 *  thread-based model. Except that this model will let me do what I
	 *  want much easier.
	 *
	 *  TODO
	 *  * Timer delay dependent on game level
	 *  * Event loop uses pselect() to wait for input from user
	 *  * Move this event code to new file events.c
	 *
	 */
	struct sigevent evp;
	memset(&evp, 0, sizeof evp);

	/* This model actually suffers from the same problem as the previous
	 * one. In the future, to solve the locking problem, I will have to
	 * write a scheduler.
	 */
	evp.sigev_notify = SIGEV_THREAD;
	evp.sigev_notify_function = blocks_tick;
	evp.sigev_value.sival_ptr = (void *)pgame;

	timer_t timerid;
	timer_create(CLOCK_MONOTONIC, &evp, &timerid);

	struct itimerspec timer = {
		.it_interval = { .tv_sec = 1, .tv_nsec = 0 },
		.it_value = { .tv_sec = 2, .tv_nsec = 0 }
	};

	timer_settime(timerid, 0, &timer, NULL);

	screen_draw_game(pgame);
	blocks_update_ghost_block(pgame, pgame->ghost);

	while (1) {
		int ch;
		ch = getch();
		blocks_input(pgame, ch);
		screen_draw_game(pgame);

		if (pgame->quit || pgame->lose || pgame->win)
			break;
	}

	if (pgame->lose) {
		db_save_score(pgame);
		screen_draw_over();
	} else {
		db_save_state(pgame);
	}

	return 0;
}
