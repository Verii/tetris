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

/* Game over screen */
void
draw_over (struct db_info *psave)
{
	if (!pgame || !psave)
		return;

	if (pgame->quit) {
		/* Don't print high scores if the user quits */
		db_save_state (pgame);
		return;
	}

	log_info ("Game over");

	/* DB access is probably the slowest operation in this program.
	 * Especially when the DB starts to get large with many saves and many
	 * high scores.
	 *
	 * refresh() the screen before we access the DB to make it feel faster
	 * to the luser
	 */

	clear ();
	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);

	mvprintw (1, 1, "Local Leaderboard");
	mvprintw (2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw (LINES-2, 1, "Press any key to quit.");
	refresh ();

	/* Save the game scores if we lost */
	if (pgame->loss)
		db_save_score (pgame);

	/* Get 10 highscores from DB */
	struct db_results *res = db_get_scores (10);

	while (res) {
		/* convert DB unix time to string */
		char *date = ctime (&res->date);
		static uint8_t count = 0;
		count ++;

		mvprintw (count+2, 4, "%2d.\t%-16s%-5d\t%-5d\t%.*s", count,
			res->id, res->level, res->score, strlen (date)-1, date);
		res = res->entries.tqe_next;
	}
	refresh (); // display scores

	db_clean_scores ();
	free (psave->file_loc);

	getch ();
}
