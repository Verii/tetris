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

	cbreak ();
	noecho ();
	nonl ();
	curs_set (0);
	keypad (stdscr, true);

	board = newwin (BLOCKS_ROWS-1, BLOCKS_COLUMNS+2, 1, 17);
	control = newwin (20, 16, 1, 1);

	start_color ();
	for (size_t i = 0; i < LEN(colors); i++)
		init_pair (i+1, colors[i], COLOR_BLACK);

	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);
	refresh ();
}

void
curses_clean (void)
{
	log_info ("Cleaning ncurses context");
	clear ();

	delwin (board);
	delwin (control);
	endwin ();
}

void
draw_menu (void)
{
	debug ("Drawing main menu");
	clear ();

	attrset (COLOR_PAIR(1));

	box (stdscr, 0, 0);
	attrset (COLOR_PAIR(7));

	mvprintw (1, 1, "Tetris-" VERSION);

	mvprintw (3, 3, "1. New Game");
	mvprintw (4, 3, "2. Resume [TODO]");
	mvprintw (5, 3, "3. Multi Player [TODO]");
	mvprintw (6, 3, "4. High Scores");
	mvprintw (7, 3, "5. Settings [TODO]");
	mvprintw (8, 3, "6. Quit");

	refresh ();
}

void
draw_highscores (void)
{
	log_info ("Drawing Highscores");

	clear ();
	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);

	mvprintw (1, 1, "Local Leaderboard");
	mvprintw (2, 3, "Rank\tName\t\tLevel\tScore\tDate");

	/* Get 10 highscores from DB */
	struct db_save *res = db_get_scores (10);
	uint8_t count = 1;

	while (res) {
		/* convert DB unix time to string */
		char date[256];
		strftime (date, sizeof date, "%B %d %Y %H:%M",
				localtime (&res->date));

		mvprintw (count+2, 3, "%0.2d.\t%-*s%-5d\t%-5d\t%.*s", count,
			sizeof res->save.id, res->save.id, res->save.level,
			res->save.score, sizeof date, date);

		++count;
		res = res->entries.tqe_next;
	}

	mvprintw (LINES-2, 1, "Press any key to continue");

	refresh (); // display scores
	getch ();

	db_clean_scores ();
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
	debug ("Drawing settings");

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

void
draw_resume (void)
{
	log_info ("Drawing resume menu");

	clear ();
	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);

	mvprintw (1, 1, "Local Leaderboard");
	mvprintw (2, 3, "Rank\tName\t\tLevel\tScore\tDate");

	/* Get 10 highscores from DB */
	struct db_save *res = db_get_scores (10);
	uint8_t count = 1;

	while (res) {
		/* convert DB unix time to string */
		char date[256];
		strftime (date, sizeof date, "%B %d %Y %H:%M",
				localtime (&res->date));

		mvprintw (count+2, 3, "%0.2d.\t%-*s%-5d\t%-5d\t%.*s", count,
			sizeof res->save.id, res->save.id, res->save.level,
			res->save.score, sizeof date, date);

		++count;
		res = res->entries.tqe_next;
	}

	refresh ();

	db_clean_scores ();
}
