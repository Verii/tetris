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
#include "helpers.h"

#include "net/serialize.h"
#include "net/network.h"
#include "net/pack.h"

tetris *pgame;
struct config *config;

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

int network_in_handler(events *pev)
{
	char buf[256];

	if (read(pev->fd, buf, sizeof buf -1) <= 0) {
		logs_to_game("You won?");
		logs_to_game("Server closed connection.");
		events_remove_IO(pev->fd);
	}

	buf[sizeof buf -1] = '\0';

	char *nl;
	if ((nl = strchr(buf, '\n')) != NULL)
		*nl = '\0';

	if (strlen(buf))
		logs_to_game(buf);

	return 1;
}

int keyboard_handler(events *pev)
{
	int ret = -1;

	struct {
		struct key_bindings *key;
		int cmd;
	} actions[] = {
		{ &config->move_drop,	TETRIS_MOVE_DROP },
		{ &config->move_down,	TETRIS_MOVE_DOWN },
		{ &config->move_left,	TETRIS_MOVE_LEFT },
		{ &config->move_right,	TETRIS_MOVE_RIGHT },
		{ &config->rotate_left,	TETRIS_ROT_LEFT },
		{ &config->rotate_right,TETRIS_ROT_RIGHT },

		{ &config->hold_key,	TETRIS_HOLD_BLOCK },
		{ &config->quit_key,	TETRIS_QUIT_GAME },
		{ &config->pause_key,	TETRIS_PAUSE_GAME },
	};

	char kb_key;
	read(pev->fd, &kb_key, 1);

	size_t i = 0;
	for (; i < LEN(actions); i++) {
		if (actions[i].key->key != kb_key)
			continue;

		if (!actions[i].key->enabled)
			continue;

		ret = tetris_cmd(pgame, actions[i].cmd);
		screen_draw_game(pgame);
		break;
	}

	if (i >= LEN(actions))
		return 0;

	return ret;
}

void timer_handler(int sig)
{
	extern volatile sig_atomic_t tetris_do_tick;
	tetris_do_tick = sig;
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

	/* Create ncurses context, draw screen, and watch for keyboard input */
	screen_init();
	screen_draw_game(pgame);
	events_add_input(fileno(stdin), keyboard_handler);

	/* Add network FD to events watch list */
	events_add_input(
			network_init(hflag? hostname: config->hostname.val,
				pflag? port: config->port.val)
			, network_in_handler);

	struct timespec ts_tick;
	ts_tick.tv_sec = 0;
	tetris_get_attr(pgame, TETRIS_GET_DELAY, &ts_tick.tv_nsec);

	struct sigaction sa_tick;
	sa_tick.sa_flags = 0;
	sa_tick.sa_handler = timer_handler;
	sigemptyset(&sa_tick.sa_mask);

	events_add_timer(ts_tick, sa_tick, SIGRTMIN+2);

	events_main_loop(pgame);

	screen_draw_over(pgame);

	tetris_cleanup(pgame);
	conf_cleanup(config);

	return 0;
}
