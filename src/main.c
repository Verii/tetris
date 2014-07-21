#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "screen.h"

static FILE *err_tofile;
static struct block_game game;

static void
redirect_stderr (void)
{
	struct stat sb;
	char log_file[128];

	snprintf (log_file, sizeof log_file, "%s/.local/share/tetris",
			getenv("HOME"));

	errno = 0;
	stat (log_file, &sb);

	/* segfaults if ~/.local/share doesn not exist */
	if (errno == ENOENT) {
		mkdir (log_file, 0777);
	}

	/* add name of log file to string */
	strcat (log_file, "/logs");

	printf ("Redirecting stderr to %s.\n", log_file);
	err_tofile = freopen (log_file, "w", stderr);
}

static void
usage (void)
{
	extern const char *__progname;
	fprintf (stderr, "Copyright (C) 2014 James Smith <james@theta.pw>\n"
		"See the LICENSE file included with the release\n\n"
		"%s-" VERSION " usage:\n\t"
		"[-h] this help\n\t"
		"[-s] show highscores\n"
		, __progname);
}

/* We can exit() at any point and still safely cleanup */
static void
cleanup (void)
{
	log_err ("Cleanup .. ");
	screen_cleanup ();
	blocks_cleanup (&game);

	printf ("Thanks for playing Blocks-" VERSION "!\n");
}

int
main (int argc, char **argv)
{
	int ch, scores_flag = 0;
	srand (time (NULL));
	setlocale (LC_ALL, "");

	while ((ch = getopt (argc, argv, "hs")) != -1) {
		switch (ch) {
		case 's':
			scores_flag = 1;
			break;
		case 'h':
		default:
			usage();
			exit (EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	redirect_stderr ();

	/* Create game context */
	if (blocks_init (&game) > 0) {
		printf ("Game successfully initialized\n");
	} else {
		printf ("Failed to initialize game\nExiting ..\n");
		exit (2);
	}

	/* create ncurses display */
	screen_init ();
	atexit (cleanup);

	struct db_info save;
	screen_draw_menu (&game, &save);

	/* Show highscores and quit. */
	if (scores_flag) {
		screen_draw_over (&game, &save);
		return (EXIT_SUCCESS);
	}

	/* draw game here so it's faster to the user */
	screen_draw_game (&game);

	pthread_t screen_loop;
	pthread_create (&screen_loop, NULL, screen_main, &game);

	/* main loop */
	blocks_loop (&game);

	/* when blocks_loop returns, kill the input thread and cleanup */
	pthread_cancel (screen_loop);

	/* Print scores, tell user they're a loser, etc. */
	screen_draw_over (&game, &save);
	fclose (err_tofile);

	return (EXIT_SUCCESS);
}
