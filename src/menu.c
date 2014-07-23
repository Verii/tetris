#include <ctype.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "menu.h"

WINDOW *board, *control;
const char colors[] = { COLOR_WHITE, COLOR_RED, COLOR_GREEN,
		COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN };

void
curses_init (void)
{
	log_info ("Initializing ncurses context");
	initscr ();

	keypad (stdscr, true);
	cbreak ();
	noecho ();
	nonl ();
	curs_set (0);

	board = newwin (BLOCKS_ROWS-1, BLOCKS_COLUMNS+2, 1, 17);
	control = newwin (16, 16, 1, 1);

	start_color ();
	for (size_t i = 0; i < LEN(colors); i++)
		init_pair (i+1, colors[i], COLOR_BLACK);

	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);
	refresh ();
}

void
curses_kill (void)
{
	log_info ("Cleaning ncurses context");
	clear ();
	endwin ();
}

/* Ask user for difficulty and their name */
void
draw_menu (void)
{
	log_info ("Drawing main menu");
	clear ();

	attrset (COLOR_PAIR(1));

	box (stdscr, 0, 0);
	attrset (COLOR_PAIR(7));

	mvprintw (1, 1, "Tetris-" VERSION);

	mvprintw (3, 3, "1. Single Player");
	mvprintw (4, 3, "2. Multi Player [TODO]");
	mvprintw (5, 3, "3. High Scores [TODO]");
	mvprintw (6, 3, "4. Settings");
	mvprintw (7, 3, "5. Quit");

	refresh ();
}

static void
get_string (int y, int x, char *str, int len)
{
	echo ();
	curs_set (1);

	attrset (COLOR_PAIR(7) | A_BOLD);
	mvgetnstr (y, x, str, len);

	curs_set (0);
	noecho ();
}

static void
get_int (int y, int x, uint8_t *val)
{
	char *endptr, number[16];

	echo ();
	curs_set (1);

	attrset (COLOR_PAIR(7) | A_BOLD);
	mvgetnstr (y, x, number, sizeof number);

	*val = strtol (number, &endptr, 10);

	curs_set (0);
	noecho ();
}

void
draw_settings (void)
{
	clear ();

	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);

	attrset (COLOR_PAIR(7));

	mvprintw (1, 1, "Tetris-" VERSION);
	mvprintw (3, 2, "Settings");

	mvprintw (5, 3, "1. Player Name: ");
	mvprintw (6, 3, "2. Difficulty: ");
	mvprintw (7, 3, "3. Size: [TODO]");
	mvprintw (8, 3, "4. Back");

	refresh ();

	int ch;
	while ((ch = getch()) != '4') {
		switch (ch) {
		case '1':
			get_string (5, 3 + strlen ("1. Player Name: "),
				pgame->id, sizeof pgame->id);
			break;
		case '2':
			get_int (6, 3 + strlen ("2. Difficulty: "),
				&pgame->diff);
			break;
		}
	}
}
