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

/* Non-standard BSD extensions */
#include <bsd/string.h>

#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "db.h"
#include "conf.h"
#include "logs.h"
#include "blocks.h"
#include "screen.h"

static const char *LICENSE =
"Copyright (C) 2014 James Smith <james@apertum.org>\n\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n";

/* We can exit() at any point and still safely cleanup */
static void cleanup(void)
{
	screen_cleanup();
//	network_cleanup();
	db_cleanup();
	blocks_cleanup();
	logs_cleanup();

	printf("Thanks for playing Tetris-%s!\n", VERSION);
}

static void usage(void)
{
	extern const char *__progname;

	fprintf(stderr, "%s\nBuilt on %s at %s\n"
		"%s-%s usage:\n\t"
		"[-u] usage\n\t"
		"[-c path] path to use for configuration file\n\t"
		"[-s] don't connect to server (single player)\n\t"
		"[-h host] hostname to connect to\n\t"
		"[-p port] port to connect to\n",
		LICENSE, __DATE__, __TIME__, __progname, VERSION);

	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");
	srand(time(NULL));

	/* Quit if we're not attached to a tty */
	if (!isatty(fileno(stdin)))
		exit(EXIT_FAILURE);

	int ch;
	while ((ch = getopt(argc, argv, "c:h:p:su")) != -1) {
		switch(ch) {
		case 'c':
			/* update search location for configuration file */
			break;
		case 'h':
			/* change default host name */
			break;
		case 'p':
			/* change default port number */
			break;
		case 's':
			/* play singleplayer */
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
	conf_init(NULL);

	if ((logs_init(NULL) != 1) ||
	    (blocks_init() != 1) ||
	    (db_init(NULL) != 1) ||
//	    (network_init(host, port) != 1) ||
	    (screen_init() != 1))
		exit(EXIT_FAILURE);

	log_info("Turn on debugging flags for more output.");

	/* Initial screen update. blocks_loop() will sleep for ~1 second before
	 * updating the screen. */
	screen_draw_game();

	atexit(cleanup);

	pthread_t input_loop;
	pthread_create(&input_loop, NULL, blocks_input, NULL);

//	pthread_t network_loop;
//	pthread_create(&network_loop, NULL, network_input, NULL);

	/* Enter game main loop */
	blocks_loop(NULL);

	/* when blocks_loop returns, kill the input thread and cleanup */
	pthread_cancel(input_loop);
//	pthread_cancel(network_loop);

	if (pgame->lose) {
		db_save_score();
		screen_draw_over();
	} else {
		db_save_state();
	}

	return 0;
}
