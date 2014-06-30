#include <ncurses.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
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
	init_pair (1, COLOR_BLUE, COLOR_BLACK);
	init_pair (2, COLOR_GREEN, COLOR_BLACK);
	init_pair (3, COLOR_WHITE, COLOR_BLACK);

	attr_t attr_text, attr_blocks, attr_border;
	attr_border = A_BOLD | COLOR_PAIR(1);
	attr_blocks = COLOR_PAIR(2);
	attr_text = A_BOLD | COLOR_PAIR(3);

	pthread_mutex_lock (&pgame->lock);
	for (int i = 2; i < BLOCKS_ROWS; i++) {
		attrset (attr_border);
		printw ("*");

		attrset (attr_blocks);
		for (int j = 0; j < BLOCKS_COLUMNS; j++) {

			if (pgame->spaces[i][j] == true)
				printw ("\u00A4");
			else if (j % 2)
				printw (".");
			else
				printw (" ");
		}
		attrset (attr_border);
		printw ("*\n");
	}
	pthread_mutex_unlock (&pgame->lock);

	for (int i = 0; i < BLOCKS_COLUMNS+2; i++)
		printw ("*");
	printw ("\n");

	attrset (attr_text);
	printw ("Current Block: %s\n",
		pgame->cur ? names[pgame->cur->type].name : "...");
	printw ("Next Block: %s\n",
		pgame->next ? names[pgame->next->type].name : "...");
	printw ("Difficulty: %s\n", levels[pgame->mod].name);
	printw ("Level: %d\n", pgame->level);
	printw ("Score: %d\n", pgame->score);
	refresh ();
}

void
screen_cleanup (void)
{
	game_over ();

	log_info ("%s", "Destroying ncurses context");
	endwin ();
}

/* Get user input, redraw game */
void *
screen_main (void *vp)
{
	struct block_game *pgame = vp;

	log_info ("%s", "Creating ncurses context");

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
		case ' ':
		case KEY_DOWN:
			cmd = MOVE_DROP;
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
