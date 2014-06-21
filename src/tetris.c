#include <locale.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"

#ifndef LOGDIR
#define LOGDIR "../logs/"
#endif

static struct blocks_game game;
static FILE *stderr_out;

static void
usage (void)
{
	extern const char *__progname;

	fprintf (stderr, "%s: usage\n\t"
			"-l [file] - specify a log file (/dev/null)\n\t"
			"-c [file] - specify a config file NOT DONE\n",
			__progname);
	exit (1);
}

static void
game_cleanup (void)
{
	endwin ();
	destroy_blocks (&game);
	fclose (stderr_out);

	/* TODO: compress file */

	const char *tank =
		"_________\n"
		"|\"\"\"\"\"\"\"\"|==========[]\n"
		"|________\\_________\n"
		"|==+===============\\\n"
		"|__________________|\n"
		"\\(@)(@)(@)(@)(@)(@)/\n";

	printf ("%s\nThanks for playing\n", tank);
}

static void
game_draw (void)
{
	clear ();

	/* Don't ever modify game, just read it */
	const struct blocks_game *s_game = &game;

	attr_t attr_text, attr_blocks, attr_border;

	init_pair (1, COLOR_BLUE, COLOR_BLACK);
	init_pair (2, COLOR_GREEN, COLOR_BLACK);
	init_pair (3, COLOR_WHITE, COLOR_BLACK);

	attr_border = A_BOLD | COLOR_PAIR(1);
	attr_blocks = COLOR_PAIR(2);
	attr_text = A_BOLD | COLOR_PAIR(3);

	pthread_mutex_lock (&game.lock);

	for (int i = 0; i < BLOCKS_ROWS; i++) {
		attron (attr_border);
		printw ("*");

		attron (attr_blocks);
		for (int j = 0; j < BLOCKS_COLUMNS; j++) {

			if (s_game->spaces[i][j] == true)
				printw ("\u00A4");
			else if (j % 2)
				printw (".");
			else
				printw (" ");
		}

		attron (attr_border);
		printw ("*\n");
	}

	pthread_mutex_unlock (&game.lock);

	for (int i = 0; i < BLOCKS_COLUMNS+2; i++)
		printw ("*");
	printw ("\n");

	attrset (attr_text);
	printw ("Play TETRIS!\n");

	printw ("Difficulty: %d\n", s_game->mod);
	printw ("Level: %d\n", s_game->level);
	printw ("Score: %d\n", s_game->score);
	printw ("Time: %d\n", s_game->time);

	refresh ();
}

static void
game_init (void)
{
	atexit (&game_cleanup);
	init_blocks (&game);

	/* ncurses init */
	initscr ();
	cbreak ();
	noecho ();
	nonl ();
	intrflush (stdscr, FALSE);
	keypad (stdscr, TRUE);
	start_color ();
}

int
main (int argc, char **argv)
{
	int ch, l_flag, c_flag;
	char log_file[80] = LOGDIR "tetris.log";

	setlocale (LC_ALL, "");

	/* ISO 8859 Tetris, technically .. */
	printf ("ASCII Tetris " VERSION "\n");

	ch = l_flag = c_flag = 0;
	while ((ch = getopt (argc, argv, "c:l:h")) != -1)
		switch (ch) {
		case 'c':
			/* TODO */
			c_flag = 1;
			if (optarg) {
				;
			}
			break;
		case 'l':
			l_flag = 1;
			if (optarg) {
				strncpy (log_file, optarg, sizeof(log_file)-1);
				log_file[sizeof(log_file)-1] = '\0';
			}
			break;
		case 'h':
		case '?':
		default:
			usage ();
			break;
		}

	argc -= optind;
	argv += optind;

	stderr_out = freopen (log_file, "w", stderr);
	if (stderr_out == NULL)
		exit (2);

	game_init ();
	game_draw ();

	/* XXX Dummy wait */
	while (getch ()) {
		int x = rand () % BLOCKS_COLUMNS;
		int y = rand () % BLOCKS_ROWS;
		game.spaces[y][x] = true;
		game_draw ();
	}

	return 0;
}
