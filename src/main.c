#define _GNU_SOURCE

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

static FILE *err_tofile;

static void
redirect_stderr (void)
{
	char *log_file;
	asprintf (&log_file, "%s/.local/share/tetris", getenv ("HOME"));

	errno = 0;

	struct stat sb;
	stat (log_file, &sb);
	if (errno == ENOENT)
		mkdir (log_file, 0777);

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
	fprintf (stderr, "%s-" VERSION " : usage",
			__progname);

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
	pthread_create (&screen_loop, NULL, screen_main, &game);

	/* Main loop */
	blocks_loop (&game);
	pthread_cancel (screen_loop);

	/* Print scores, tell user they're a loser, etc. */
	screen_draw_over (&game, &save);
	screen_cleanup ();
	blocks_cleanup (&game);

	fclose (err_tofile);
	printf ("Thanks for playing \"Falling Blocks Game\"-" VERSION "!\n");

	return (EXIT_SUCCESS);
}
