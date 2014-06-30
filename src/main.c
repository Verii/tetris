#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"
#include "screen.h"

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
	int ch, l_flag;
	FILE *stderr_out;
	pthread_t screen_loop;
	struct block_game game;
	char log_file[80] = LOGDIR "game.log";

	setlocale (LC_ALL, "");

	ch = l_flag = 0;
	while ((ch = getopt (argc, argv, "l:h")) != -1) {
		switch (ch) {
		case 'l':
			l_flag = 1;
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

	init_blocks (&game);
	pthread_create (&screen_loop, NULL, screen_main, &game);
	loop_blocks (&game);

	pthread_cancel (screen_loop);
	screen_cleanup ();
	cleanup_blocks (&game);

	fclose (stderr_out);

	printf ("Thanks for playing!\n");
	return EXIT_SUCCESS;
}
