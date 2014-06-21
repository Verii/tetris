#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <locale.h>
#include <ncurses.h>

#include "blocks.h"
#include "debug.h"

#ifndef LOGDIR
#define LOGDIR "../logs/"
#endif

static struct blocks_game game;
static FILE *stderr_out;

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

	attr_t attr_text, attr_blocks, attr_border;

	init_pair (1, COLOR_BLUE, COLOR_BLACK);
	init_pair (2, COLOR_GREEN, COLOR_BLACK);
	init_pair (3, COLOR_WHITE, COLOR_BLACK);

	attr_border = A_BOLD | COLOR_PAIR(1);
	attr_blocks = COLOR_PAIR(2);
	attr_text = A_BOLD | COLOR_PAIR(3);

	for (int i = 0; i < BLOCKS_ROWS; i++) {
		attron (attr_border);
		printw ("*");

		attron (attr_blocks);
		for (int j = 0; j < BLOCKS_COLUMNS; j++) {

			if (game.spaces[i][j] == true)
				printw ("\u00A4");
			else if (j % 2)
				printw (".");
			else
				printw (" ");
		}

		attron (attr_border);
		printw ("*\n");
	}

	for (int i = 0; i < BLOCKS_COLUMNS+2; i++)
		printw ("*");
	printw ("\n");

	attrset (attr_text);
	printw ("Play TETRIS!\n");

	refresh ();
}

static void
game_init (void)
{
	atexit(&game_cleanup);
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
	setlocale (LC_ALL, "");

	/* ISO 8859 Tetris, technically .. */
	printf ("ASCII Tetris " VERSION "\n");

	/* TODO */
	int ch;
	while ((ch = getopt(argc, argv, "c:l:")) != -1)
		switch (ch) {
		case 'c':
			break;
		case 'l':
			break;
		case '?':
		default:
			break;
		}

	stderr_out = freopen (LOGDIR "tetris.log", "w", stderr);
	if (stderr_out == NULL)
		exit (2);

	game_init ();
	game_draw ();

	/* XXX Dummy wait */
	while (getch()) {
		int x = rand() % BLOCKS_COLUMNS;
		int y = rand() % BLOCKS_ROWS;
		game.spaces[y][x] = true;
		game_draw ();
	}

	return 0;
}
