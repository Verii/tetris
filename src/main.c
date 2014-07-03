#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "db.h"
#include "screen.h"

static void
usage (void)
{
	extern const char *__progname;
	fprintf (stderr, "%s-" VERSION " : usage\n\t"
			"-h - usage\n",
			__progname);
	exit (EXIT_FAILURE);
}

int
main (int argc, char **argv)
{
	FILE *stderr_out;
	pthread_t screen_loop;
	struct block_game game;
	struct db_info save;
	char log_file[80] = { 0 };

	setlocale (LC_ALL, "");

	if (argc > 1 && argv[1][0] == '-' && argv[1][1] == 'h')
		usage ();

	strncat (log_file, getenv("HOME"), sizeof log_file);
	strncat (log_file, "/.local/share/tetris/game.log" , sizeof log_file);

	stderr_out = freopen (log_file, "w", stderr);

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

	fclose (stderr_out);
	printf ("Thanks for playing \"Falling Blocks Game\"-" VERSION "!\n");

	return (EXIT_SUCCESS);
}
