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

struct block_names names[] = {
	[SQUARE_BLOCK]	= { SQUARE_BLOCK,	"Square" },
	[LINE_BLOCK]	= { LINE_BLOCK,		"Line" },
	[T_BLOCK]	= { T_BLOCK,		"T" },
	[L_BLOCK]	= { L_BLOCK,		"L" },
	[L_REV_BLOCK]	= { L_REV_BLOCK,	"L_rev" },
	[Z_BLOCK]	= { Z_BLOCK,		"Z" },
	[Z_REV_BLOCK]	= { Z_REV_BLOCK,	"Z_rev" },
};

struct level_names levels[] = {
	[DIFF_EASY]	= { DIFF_EASY,		"Easy" },
	[DIFF_NORMAL]	= { DIFF_NORMAL,	"Normal" },
	[DIFF_HARD]	= { DIFF_HARD,		"Hard" },
};

/* Game over screen */
static void
game_over (void)
{
	return;
}

/* Ask user for difficulty and their name */
static void
user_menu (void)
{
	return;
}

void
screen_draw (struct block_game *pgame)
{
	clear ();

	int colors[] = { COLOR_BLUE, COLOR_GREEN, COLOR_WHITE, COLOR_YELLOW,
		COLOR_RED, COLOR_MAGENTA, COLOR_CYAN };

	for (size_t i = 0; i < LEN(colors); i++) {
		init_pair (i, colors[i], COLOR_BLACK);
	}

	attr_t attr_text, attr_border;
	attr_border = A_BOLD | COLOR_PAIR(0);
	attr_text = A_BOLD | COLOR_PAIR(2);

	pthread_mutex_lock (&pgame->lock);
	for (int i = 2; i < BLOCKS_ROWS; i++) {
		attrset (attr_border);
		printw ("*");

		attrset (0);
		for (int j = 0; j < BLOCKS_COLUMNS; j++) {

			if (pgame->spaces[i][j] == true) {
				attron (COLOR_PAIR(i % LEN(colors)));
				printw ("\u00A4");
				attroff (COLOR_PAIR(i % LEN(colors)));
			} else if (j % 2)
				printw (".");
			else
				printw (" ");
		}
		attrset (attr_border);
		printw ("*\n");
	}
	for (int i = 0; i < BLOCKS_COLUMNS+2; i++)
		printw ("*");
	printw ("\n");

	attrset (attr_text);

	/* Just after moving a piece, before the tick thread can take over, a
	 * block might be removed from hitting something.
	 * Print the next piece as the current piece.
	 */
	if (pgame->cur == NULL) {
		printw ("Current Block: %s\n", names[pgame->next->type].name);
		printw ("Next Block: ...\n");
	} else {
		printw ("Current Block: %s\n", names[pgame->cur->type].name);
		printw ("Next Block: %s\n",
			pgame->next ? names[pgame->next->type].name : "...");
	}

	if (pgame->save)
		printw ("Save Block: %s\n", names[pgame->save->type].name);

	printw ("Difficulty: %s\n", levels[pgame->mod].name);
	printw ("Level: %d\n", pgame->level);
	printw ("Score: %d\n", pgame->score);
	pthread_mutex_unlock (&pgame->lock);

	refresh ();
}

void
screen_cleanup (struct block_game *pgame)
{
	game_over ();
	log_info ("Cleaning ncurses context");
	endwin ();

	/* TODO */
	struct db_info scores;
	scores.file_loc = "../saves/tetris.db";

	db_add_game_score (&scores, pgame);
}

/* Get user input, redraw game */
void *
screen_main (void *vp)
{
	struct block_game *pgame = vp;

	log_info ("Initializing ncurses context");

	/* init curses */
	initscr ();
	cbreak ();
	noecho ();
	nonl ();
	intrflush (stdscr, FALSE);
	keypad (stdscr, TRUE);
	start_color ();

	user_menu ();
	screen_draw (pgame);

	int ch;
	while ((ch = getch()) != EOF) {
		enum block_cmd cmd;

		if (ch == 'P')
			pgame->game_over = true;

		switch (ch) {
		case 'a':
		case 'A':
		case KEY_LEFT:
			cmd = MOVE_LEFT;
			break;
		case 'd':
		case 'D':
		case KEY_RIGHT:
			cmd = MOVE_RIGHT;
			break;
		case 's':
		case 'S':
		case KEY_DOWN:
			cmd = MOVE_DROP;
			break;
		case ' ':
			cmd = SAVE_PIECE;
			break;
		case 'q':
		case 'Q':
			cmd = ROT_LEFT;
			break;
		case 'e':
		case 'E':
			cmd = ROT_RIGHT;
			break;
		default:
			continue;
		}

		move_blocks (pgame, cmd);
	}

	return NULL;
}
