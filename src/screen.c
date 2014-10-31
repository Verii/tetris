/*
 * Copyright (C) 2014  James Smith <james@theta.pw>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <bsd/string.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "screen.h"

#define GAME_Y_OFF 2
#define GAME_X_OFF 2

#define TEXT_Y_OFF 2
#define TEXT_X_OFF (BLOCKS_MAX_COLUMNS + GAME_X_OFF + 2)

#define BLOCK_CHAR "x"

static WINDOW *board;

static const char colors[] = { COLOR_WHITE, COLOR_RED, COLOR_GREEN,
	COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN
};

void screen_init(void)
{
	log_info("Initializing ncurses context");
	initscr();

	cbreak();
	noecho();
	nonl();
	keypad(stdscr, TRUE);
	curs_set(0);

	start_color();
	for (size_t i = 0; i < LEN(colors); i++)
		init_pair(i + 1, colors[i], COLOR_BLACK);

	board = newwin(0, 0, 0, 0);

	wattrset(board, COLOR_PAIR(1));
	box(board, 0, 0);

	/* Draw static text */
	mvwprintw(board, 1, 1, "Tetris-" VERSION);
	mvwprintw(board, TEXT_Y_OFF +5, TEXT_X_OFF +1, "Hold   Next");
	mvwprintw(board, TEXT_Y_OFF +9, TEXT_X_OFF +1, "Controls");
	mvwprintw(board, TEXT_Y_OFF +10, TEXT_X_OFF +2, "Pause [F1]");
	mvwprintw(board, TEXT_Y_OFF +11, TEXT_X_OFF +2, "Quit [F3]");
	mvwprintw(board, TEXT_Y_OFF +13, TEXT_X_OFF +2, "Move [asd]");
	mvwprintw(board, TEXT_Y_OFF +14, TEXT_X_OFF +2, "Rotate [qe]");
	mvwprintw(board, TEXT_Y_OFF +15, TEXT_X_OFF +2, "Hold [[space]]");

	/* Draw board outline */
	wattrset(board, A_BOLD | COLOR_PAIR(5));

	mvwvline(board, GAME_Y_OFF, GAME_X_OFF, '*', BLOCKS_MAX_ROWS -1);

	mvwvline(board, GAME_Y_OFF, BLOCKS_MAX_COLUMNS +1 +GAME_X_OFF,
			'*', BLOCKS_MAX_ROWS -1);
	mvwhline(board, BLOCKS_MAX_ROWS -2 +GAME_Y_OFF, GAME_X_OFF,
			'*', BLOCKS_MAX_COLUMNS +2);

	refresh();
}

void screen_cleanup(void)
{
	log_info("Cleaning ncurses context");
	delwin(board);
	clear();
	endwin();
}

/* Ask user for difficulty and their name */
void screen_draw_menu(struct db_info *psave)
{
	const size_t buf_len = 256;

	memset(psave, 0, sizeof *psave);

	/* TODO place holder name, get from user later */
	strlcpy(psave->id, "Lorem Ipsum", sizeof psave->id);

	psave->file_loc = calloc(1, buf_len);
	if (psave->file_loc == NULL) {
		log_err("Out of memory");
		exit(EXIT_FAILURE);
	}

	/* Create an in-memory DB when debugging so we don't mess
	 * with our saves
	 */
	snprintf(psave->file_loc, buf_len,
#if defined(DEBUG) || !defined(NDEBUG)
		":memory:"
#else
		"%s/.local/share/tetris/saves", getenv("HOME")
#endif
		);

	/* Start the game paused if we can resume from an old save */
	if (db_resume_state(psave, pgame) > 0) {
		pgame->pause = true;
	}
}

void screen_draw_game(void)
{
	size_t i, j;

	wattrset(board, COLOR_PAIR(1));
	mvwprintw(board, TEXT_Y_OFF +2, TEXT_X_OFF +1,
			"Level %d", pgame->level);
	mvwprintw(board, TEXT_Y_OFF +3, TEXT_X_OFF +1,
			"Score %d", pgame->score);

	/* Center the text horizontally, place the text slightly above
	 * the middle vertically.
	 */
	if (pgame->pause) {
		wattrset(board, COLOR_PAIR(1) | A_BOLD);
		mvwprintw(board, (BLOCKS_MAX_ROWS -6) /2 +GAME_Y_OFF,
				 (BLOCKS_MAX_COLUMNS -2) /2 -1 +GAME_X_OFF,
				 "PAUSED");

		goto refresh_board;
	}

	/*************/
	/* Draw game */
	/*************/

	/* pgame->blocks_head.lh_first
	 * pgame->blocks_head.lh_first->entries.le_next
	 */

	/* Draw the background of the board. Dot every other column */
	for (i = 2; i < BLOCKS_MAX_ROWS; i++)
		mvwprintw(board, i -2 +GAME_Y_OFF, 1 +GAME_X_OFF,
				" . . . . .");

	/* Draw the game board, minus the two hidden rows above the game */
	for (i = 2; i < BLOCKS_MAX_ROWS; i++) {
		for (j = 0; j < BLOCKS_MAX_COLUMNS && pgame->spaces[i]; j++) {
			if (!blocks_at_yx(i, j))
				continue;

			wattrset(board, COLOR_PAIR((pgame->colors[i][j]) +1)
					| A_BOLD);
			mvwprintw(board, i -2 +GAME_Y_OFF, j +1 +GAME_X_OFF,
					BLOCK_CHAR);
		}
	}

refresh_board:
	wrefresh(board);
}

/* Game over screen */
void screen_draw_over(struct db_info *psave)
{
	if (!psave)
		return;

	log_info("Game over");

	clear();
	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);

	mvprintw(1, 1, "Local Leaderboard");
	mvprintw(2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw(LINES - 2, 1, "Press F1 to quit.");

	if (pgame->lose) {
		db_save_score(psave, pgame);
	} else {
		db_save_state(psave, pgame);
		return;
	}

	/* Print score board when you lose a game */
	struct db_results *res = db_get_scores(psave, 10);
	while (res) {
		char *date = ctime(&res->date);
		static unsigned char count = 0;

		count++;
		mvprintw(count + 2, 4, "%2d.\t%-16s%-5d\t%-5d\t%.*s", count,
			 res->id, res->level, res->score, strlen(date) - 1,
			 date);
		res = res->entries.tqe_next;
	}
	refresh();

	db_clean_scores();
	free(psave->file_loc);

	while (getch() != KEY_F(1)) ;
}
