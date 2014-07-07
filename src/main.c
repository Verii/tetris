#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "blocks.h"
#include "db.h"
#include "screen.h"
#include "net.h"

static FILE *err_tofile;

static void
redirect_stderr (void)
{
	char *log_file;
	if (asprintf(&log_file, "%s/.local/share/tetris", getenv("HOME")) < 0) {
		exit (2);
	}

	errno = 0;

	struct stat sb;
	stat (log_file, &sb);

	if (errno == ENOENT) {
		mkdir (log_file, 0777);
	}

	log_file = realloc (log_file, strlen (log_file)+20);
	strcat (log_file, "/game.log");

	printf ("Redirecting stderr to %s.\n", log_file);

	err_tofile = freopen (log_file, "w", stderr);
	free (log_file);
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

int
main (int argc, char **argv)
{
	pthread_t screen_loop;
	struct block_game game;
	struct db_info save;

	setlocale (LC_ALL, "");

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h')
		usage ();

	redirect_stderr ();

	/* Create game context and screen */
	blocks_init (&game);
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
	screen_cleanup ();
	blocks_cleanup (&game);

	fclose (err_tofile);
	printf ("Thanks for playing Blocks-" VERSION "!\n");

	return (EXIT_SUCCESS);
}
