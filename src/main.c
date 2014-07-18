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
#include "screen.h"

#ifndef LOG_FILE
#define LOG_FILE "/logs"
#endif

static FILE *err_tofile;

static pthread_t screen_loop;
static struct block_game game;
static struct db_info save;

static char log_file[256];

static void
redirect_stderr (void)
{
	snprintf (log_file, sizeof log_file, "%s/.local/share/tetris",
			getenv("HOME"));

	errno = 0;

	struct stat sb;
	stat (log_file, &sb);

	if (errno == ENOENT) {
		mkdir (log_file, 0777);
	}

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
	atexit (cleanup);
	setlocale (LC_ALL, "");

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h')
		usage ();

	redirect_stderr ();

	srand (time (NULL));

	/* Create game context and screen */
	if (blocks_init (&game) > 0) {
		printf ("Game successfully initialized\n");
	} else {
		printf ("Failed to initialize game\nExiting ..\n");
		exit (2);
	}

	screen_init ();

	/* Draw main menu */
	screen_draw_menu (&game, &save);
	screen_draw_game (&game);

	pthread_create (&screen_loop, NULL, screen_main, &game);

	/* Main loop */
	blocks_loop (&game);
	pthread_cancel (screen_loop);

	/* Print scores, tell user they're a loser, etc. */
	screen_draw_over (&game, &save);

	return (EXIT_SUCCESS);
}
