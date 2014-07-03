#include <ncurses.h>
#include <pthread.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "screen.h"

static const struct block_names {
	enum block_type block;
	char *name;
} names[] = {
	[SQUARE_BLOCK]	= { SQUARE_BLOCK,	"Square" },
	[LINE_BLOCK]	= { LINE_BLOCK,		"Line" },
	[T_BLOCK]	= { T_BLOCK,		"T" },
	[L_BLOCK]	= { L_BLOCK,		"L" },
	[L_REV_BLOCK]	= { L_REV_BLOCK,	"L_rev" },
	[Z_BLOCK]	= { Z_BLOCK,		"Z" },
	[Z_REV_BLOCK]	= { Z_REV_BLOCK,	"Z_rev" },
};

static const struct level_names {
	enum block_diff difficulty;
	char *name;
} levels[] = {
	[DIFF_EASY]	= { DIFF_EASY,		"Easy" },
	[DIFF_NORMAL]	= { DIFF_NORMAL,	"Normal" },
	[DIFF_HARD]	= { DIFF_HARD,		"Hard" },
};

void
screen_init (void)
{
	log_info ("Initializing ncurses context");
	initscr ();
	cbreak ();
	noecho ();
	nonl ();
	intrflush (stdscr, FALSE);
	keypad (stdscr, TRUE);
	start_color ();
}

/* Ask user for difficulty and their name */
void
screen_draw_menu (struct block_game *pgame, struct db_info *psave)
{
	/* TODO ask user for save game name, file location (with a default)
	 * and game settings:
	 * playermode (single/multi), difficulty
	 */
	psave->id = "Lorem Ipsum";
	psave->file_loc = "../saves/game.db";

	pgame->name = psave->id;
	pgame->mod = DIFF_NORMAL;

	db_resume_state (psave, pgame);
}

void
screen_draw_game (struct block_game *pgame)
{
	static const char colors[] = { COLOR_WHITE, COLOR_RED, COLOR_GREEN,
		COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN };

	for (size_t i = 0; i < LEN(colors); i++)
		init_pair (i, colors[i], COLOR_BLACK);

	attr_t text, border;
	border = A_BOLD | COLOR_PAIR(4);
	text = A_BOLD | COLOR_PAIR(0);

	pthread_mutex_lock (&pgame->lock);

	clear ();

	attrset (text);
	printw ("%s: \n\t", pgame->name);

	for (int i = 2; i < BLOCKS_ROWS; i++) {
		attrset (border);
		printw ("*");

		for (int j = 0; j < BLOCKS_COLUMNS; j++) {

			int color;
			color = pgame->colors[i][j] % sizeof (colors);
			attrset (COLOR_PAIR(color));

			char *str = " ";
			if (pgame->spaces[i][j]) {
				attron (A_BOLD);
				str = "\u00A4";
			} else if (j % 2) {
				str =  ".";
			}

			printw (str);
		}

		attrset (border);
		printw ("*\n\t");
	}

	for (int i = 0; i < BLOCKS_COLUMNS+2; i++)
		printw ("*");

	attrset (text);

	/* TODO move non-game information to a 'status' window, or something ..
	 * Include prints of current game piece, and the next piece.
	 * Include game controls, F1 to pause, F3 to save and quit,
	 * movement controls.
	 */

	/* TODO overlay PAUSE text when the game is paused */

	/* Just after moving a piece, before the tick thread can take over, a
	 * block might be removed from hitting something.
	 * Print the next piece as the current piece.
	 */

	printw ("\nCur %s\n", pgame->cur ? names[pgame->cur->type].name :
			names[pgame->next->type].name);
	printw ("Next %s\n", pgame->cur ? names[pgame->next->type].name : "");
	printw ("Save %s\n", pgame->save ? names[pgame->save->type].name : "");

	printw ("Level %d\t", pgame->level);
	printw ("Score %d\n", pgame->score);

	if (pgame->pause)
		printw ("Paused\n");

	refresh ();

	pthread_mutex_unlock (&pgame->lock);
}

/* Game over screen */
void
screen_draw_over (struct block_game *pgame, struct db_info *psave)
{
	clear ();

	/* TODO Loser screen */

	/* Save scores, or save game state */
	log_info ("Saving game");
	if (pgame->loss)
		db_save_score (psave, pgame);
	else {
		db_save_state (psave, pgame);
		return;
	}

	/* Print score board */
	int count = 0;
	struct db_results *res;
	res = db_get_scores (psave, 10);
	if (!res)
		return;

	printw (" Rank\tName\t\tLevel\tScore\tDate\n");
	while (res) {
		printw (" %2d). \t%-16s%d\t%d\t%s", ++count,
			res->id, res->level, res->score, ctime(&res->date));
		res = res->entries.tqe_next;
	}

	db_clean_scores ();

	refresh ();
	getch ();
}

void
screen_cleanup (void)
{
	log_info ("Cleaning ncurses context");
	endwin ();
}

/* Get user input, redraw game */
void *
screen_main (void *vp)
{
	struct block_game *pgame = vp;

	screen_draw_game (pgame);

	int ch;
	while ((ch = getch()) != EOF) {

		if (ch == KEY_F(3)) {
			pgame->pause = false;
			pgame->quit = true;
		}

		/* Toggle pause */
		if (ch == KEY_F(1))
			pgame->pause = !pgame->pause;

		/* Prevent movement while paused */
		if (pgame->pause) {
			screen_draw_game (pgame);
			continue;
		}

		switch (ch) {
		case 'a':
		case 'A':
		case KEY_LEFT:
			blocks_move (pgame, MOVE_LEFT);
			break;
		case 'd':
		case 'D':
		case KEY_RIGHT:
			blocks_move (pgame, MOVE_RIGHT);
			break;
		case 's':
		case 'S':
		case KEY_DOWN:
			blocks_move (pgame, MOVE_DROP);
			break;
		case ' ':
			blocks_move (pgame, SAVE_PIECE);
			break;
		case 'q':
		case 'Q':
			blocks_move (pgame, ROT_LEFT);
			break;
		case 'e':
		case 'E':
			blocks_move (pgame, ROT_RIGHT);
			break;
		default:
			break;
		}

		screen_draw_game (pgame);
	}

	return NULL;
}
