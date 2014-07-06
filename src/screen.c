#define _GNU_SOURCE

#include <ctype.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "screen.h"

static const char colors[] = { COLOR_WHITE, COLOR_RED, COLOR_GREEN,
		COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN };

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
	curs_set (0);

	for (size_t i = 0; i < LEN(colors); i++)
		init_pair (i, colors[i], COLOR_BLACK);
}

/* Ask user for difficulty and their name */
void
screen_draw_menu (struct block_game *pgame, struct db_info *psave)
{
	/* TODO ask user for save game name, file location (with a default)
	 * and game settings:
	 * playermode (single/multi), difficulty, resume
	 */
	memset (psave, 0, sizeof *psave);

	strncpy (psave->id, "Lorem Ipsum", sizeof psave->id);
	if (asprintf (&psave->file_loc, "%s/.local/share/tetris/game.db",
			getenv ("HOME")) < 0) {
		exit (2);
	}

	/* Start the game paused if we can resume from an old save */
	if (db_resume_state (psave, pgame) > 0)
		pgame->pause = true;
}

void
screen_draw_game (struct block_game *pgame)
{
	attr_t text, border;
	text = COLOR_PAIR(0);
	border = A_BOLD | COLOR_PAIR(4);

	attrset (text);
	box (stdscr, 0, 0);

	mvprintw (1, 1, "Tetris-" VERSION);
	mvprintw (3, 2, "Level %d", pgame->level);
	mvprintw (4, 2, "Score %d", pgame->score);

	mvprintw (6, 2, "Next\tSave");
	mvprintw (7, 3, "\t\t");
	mvprintw (8, 3, "\t\t");

	mvprintw (10, 2, "Controls");
	mvprintw (11, 3, "Pause [F1]");
	mvprintw (12, 3, "Quit [F3]");
	mvprintw (14, 3, "Move [asd]");
	mvprintw (15, 3, "Rotate [qe]");
	mvprintw (16, 3, "Save [space]");

	int game_x_offset = 18;
	int game_y_offset = 1;

	attrset (border);
	move (game_y_offset, game_x_offset);
	vline ('*', BLOCKS_ROWS-1);

	move (game_y_offset, game_x_offset+BLOCKS_COLUMNS+1);
	vline ('*', BLOCKS_ROWS-1);

	move ((BLOCKS_ROWS-2)+game_y_offset, game_x_offset);
	hline ('*', BLOCKS_COLUMNS+2);

	/* two hidden rows above game, where blocks spawn */
	for (int i = 2; i < BLOCKS_ROWS; i++) {
		move ((i-2)+game_y_offset, game_x_offset+1);

		for (int j = 0; j < BLOCKS_COLUMNS; j++) {
			attrset (0);
			if (pgame->spaces[i][j]) {
				attrset (COLOR_PAIR(pgame->colors[i][j]
						% sizeof colors));
				attron (A_BOLD);
				printw ("\u00A4");
			} else if (j % 2)
				printw (".");
			else
				printw (" ");
		}
	}

	for (size_t i = 0; i < LEN(pgame->next->p); i++) {
		int y, x;
		y = pgame->next->p[i].y;
		x = pgame->next->p[i].x;

		attrset (COLOR_PAIR(pgame->next->type % sizeof colors));
		attron (A_BOLD);
		mvprintw (y+8, x+4, "\u00A4");

		if (pgame->save) {
			y = pgame->save->p[i].y;
			x = pgame->save->p[i].x;

			attrset (COLOR_PAIR(pgame->save->type % sizeof colors));
			attron (A_BOLD);
			mvprintw (y+8, x+10, "\u00A4");
		}
	}

	if (pgame->pause) {
		attrset (text | A_BOLD);
		mvprintw (((BLOCKS_ROWS-2)/2)-2 +game_y_offset,
			game_x_offset+3, "PAUSED");
	}

	refresh ();
}

/* Game over screen */
void
screen_draw_over (struct block_game *pgame, struct db_info *psave)
{
	clear ();

	attrset (0);
	box (stdscr, 0, 0);

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

	mvprintw (1, 1, "Local Leaderboards");
	mvprintw (2, 3, "Rank\tName\t\tLevel\tScore\tDate\n");
	while (res) {
		count++;
		mvprintw (count+2, 4, "%2d.\t%-16s%5d\t%5d\t%s", count,
			res->id, res->level, res->score, ctime(&res->date));
		res = res->entries.tqe_next;
	}

	db_clean_scores ();
	free (psave->file_loc);

	mvprintw (LINES-2, 1, "Press F1 to quit.");
	refresh ();
	while (getch () != KEY_F(1));
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

	int ch;
	while ((ch = getch())) {

		if (ch == KEY_F(3)) {
			pgame->pause = false;
			pgame->quit = true;
		}

		if (ch == KEY_F(1))
			pgame->pause = !pgame->pause;

		enum block_cmd cmd = -1;

		switch (toupper(ch)) {
		case 'A':
			cmd = MOVE_LEFT;
			break;
		case 'D':
			cmd = MOVE_RIGHT;
			break;
		case 'S':
			cmd = MOVE_DOWN;
			break;
		case 'W':
			cmd = MOVE_DROP;
			break;
		case 'Q':
			cmd = ROT_LEFT;
			break;
		case 'E':
			cmd = ROT_RIGHT;
			break;
		case ' ':
			cmd = SAVE_PIECE;
			break;
		}

		blocks_move (pgame, cmd);
	}

	return NULL;
}
