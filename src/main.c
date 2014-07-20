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

#ifndef LOG_FILE
#define LOG_FILE "/logs"
#endif

static FILE *err_tofile;
static struct block_game game;
static char log_file[128];
static pthread_t screen_loop;

static void
redirect_stderr (void)
{
	struct stat sb;

	snprintf (log_file, sizeof log_file, "%s/.local/share/tetris",
			getenv("HOME"));

	errno = 0;
	stat (log_file, &sb);

	/* segfaults if ~/.local/share doesn not exist */
	if (errno == ENOENT) {
		mkdir (log_file, 0777);
	}

	/* add name of log file to string */
	strcat (log_file, LOG_FILE);

	printf ("Redirecting stderr to %s.\n", log_file);
	err_tofile = freopen (log_file, "w", stderr);
}

static void
usage (void)
{
	extern const char *__progname;
	fprintf (stderr, "Copyright (C) 2014 James Smith <james@theta.pw>\n"
		"See the license.txt file included with the release\n\n"
		"%s-" VERSION " usage:\n\t"
		"[-h] this help\n", __progname);

	exit (EXIT_FAILURE);
}

/* We can exit() at any point and still safely cleanup */
static void
cleanup (void)
{
	screen_cleanup ();
	blocks_cleanup (&game);

	fclose (err_tofile);
	printf ("Thanks for playing Blocks-" VERSION "!\n");
}

int
main (int argc, char **argv)
{
	setlocale (LC_ALL, "");
	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h')
		usage ();

	srand (time (NULL));
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

	screen_draw_game (&game);

	pthread_create (&screen_loop, NULL, screen_main, &game);

	/* when blocks_loop returns, kill the input thread and cleanup */
	blocks_loop (&game);
	pthread_cancel (screen_loop);

	/* Print scores, tell user they're a loser, etc. */
	screen_draw_over (&game, &save);

	return (EXIT_SUCCESS);
}
