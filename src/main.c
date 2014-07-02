#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"
#include "screen.h"
#include "db.h"

#ifndef LOGDIR
#define LOGDIR "../logs/"
#endif

static void
usage (void)
{
	extern const char *__progname;
	fprintf (stderr, "%s-" VERSION " : usage\n\t"
			"-l [file] - specify a log file (/dev/null)\n\t"
			"-c [file] - specify a config file NOT DONE\n",
			__progname);
	exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
	int ch = 0;
	FILE *stderr_out;
	pthread_t screen_loop;
	struct block_game game;
	struct db_info save;
	char log_file[80] = LOGDIR "game.log";

	setlocale (LC_ALL, "");

	while ((ch = getopt (argc, argv, "l:h")) != -1) {
		switch (ch) {
		case 'l':
			if (optarg) {
				strncpy (log_file, optarg, sizeof(log_file)-1);
				log_file[sizeof(log_file)-1] = '\0';
			}
			break;
		case 'h':
		default:
			usage ();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	stderr_out = freopen (log_file, "w", stderr);

	/* Create game context and screen */
	blocks_init (&game);
	screen_init ();

	/* Draw main menu */
	screen_draw_menu (&save);
	pthread_create (&screen_loop, NULL, screen_main, &game);

	/* Main loop */
	blocks_loop (&game);
	pthread_cancel (screen_loop);

	/* Print scores, tell user they're a loser, etc. */
	screen_draw_over (&game, &save);
	screen_cleanup ();
	blocks_cleanup (&game);

	fclose (stderr_out);
	printf ("Thanks for playing!\n");

	return EXIT_SUCCESS;
}
