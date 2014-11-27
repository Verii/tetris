/*
 * Copyright (C) 2014  James Smith <james@apertum.org>
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

#define BLOCKS_Y_OFF TEXT_Y_OFF
#define BLOCKS_X_OFF TEXT_X_OFF + 20

#define BLOCK_CHAR "x"

static WINDOW *board, *pieces;

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
	pieces = newwin(17, 14, BLOCKS_Y_OFF, BLOCKS_X_OFF);

	wattrset(board, COLOR_PAIR(1));
	box(board, 0, 0);

	/* Draw static text */
	mvwprintw(board, 1, 1, "Tetris-" VERSION);
#ifdef DEBUG
	wattrset(board, A_BOLD | COLOR_PAIR(2));
	mvwprintw(board, TEXT_Y_OFF +18, TEXT_X_OFF +1,
			"DEBUG @ %s %s", __DATE__, __TIME__);

	wattrset(board, A_BOLD | COLOR_PAIR(3));
	mvwprintw(board, TEXT_Y_OFF +19, TEXT_X_OFF +1,
			"saving to in-memory database");

	wattrset(board, COLOR_PAIR(1));
#endif

	wattrset(board, COLOR_PAIR(1));
	mvwprintw(board, TEXT_Y_OFF+1, TEXT_X_OFF+1, "Level");
	mvwprintw(board, TEXT_Y_OFF+2, TEXT_X_OFF+1, "Score");
	mvwprintw(board, TEXT_Y_OFF+3, TEXT_X_OFF+1, "Pause");

	mvwprintw(board, TEXT_Y_OFF +7, TEXT_X_OFF +1, "Controls");
	mvwprintw(board, TEXT_Y_OFF +8, TEXT_X_OFF +2, "Pause [p]");
	mvwprintw(board, TEXT_Y_OFF +9, TEXT_X_OFF +2, "Save/Quit [o]");
	mvwprintw(board, TEXT_Y_OFF +10, TEXT_X_OFF +2, "Move [asd]");
	mvwprintw(board, TEXT_Y_OFF +11, TEXT_X_OFF +2, "Rotate [qe]");
	mvwprintw(board, TEXT_Y_OFF +12, TEXT_X_OFF +2, "Ghosts [g]");
	mvwprintw(board, TEXT_Y_OFF +13, TEXT_X_OFF +2, "Hold [[space]]");

	/* Draw board outline */
	wattrset(board, A_BOLD | COLOR_PAIR(5));

	mvwvline(board, GAME_Y_OFF, GAME_X_OFF, '*', BLOCKS_MAX_ROWS -1);

	mvwvline(board, GAME_Y_OFF, BLOCKS_MAX_COLUMNS +1 +GAME_X_OFF,
			'*', BLOCKS_MAX_ROWS -1);
	mvwhline(board, BLOCKS_MAX_ROWS -2 +GAME_Y_OFF, GAME_X_OFF,
			'*', BLOCKS_MAX_COLUMNS +2);

	refresh();
	wrefresh(board);
}

void screen_cleanup(void)
{
	log_info("Cleaning ncurses context");
	delwin(board);
	delwin(pieces);
	clear();
	endwin();
}

/* Ask user for difficulty and their name */
void screen_draw_menu(void)
{
	const size_t buf_len = 256;

	memset(psave, 0, sizeof *psave);

	/* TODO place holder name, get from user later */
	strlcpy(psave->id, getenv("USER"), sizeof psave->id);

	psave->file_loc = calloc(1, buf_len);
	if (!psave->file_loc) {
		log_err("Out of memory");
		exit(EXIT_FAILURE);
	}

	/* Set location of DB file on disk.
	 * We already know that $HOME is safe from the environment.
	 * Technically, the user could redefine the HOME variable in the short time
	 * since the previous access. But if that was to happen, all they would get
	 * is a single-session save.
	 */
	snprintf(psave->file_loc, buf_len, "%s/.local/share/tetris/saves",
			getenv("HOME"));

	/* Create an in-memory DB when debugging so we don't mess
	 * with our saves
	 */
#ifdef DEBUG
	strncpy(psave->file_loc, ":memory:", buf_len);
#endif

	/* Start the game paused if we can resume from an old save */
	if (db_resume_state() > 0) {
		pgame->pause = true;
	}
}

/* Draw the Hold and Next blocks listing on the side of the game. */
void screen_draw_blocks(void)
{
	struct blocks *np;
	size_t i, count = 0;

	werase(pieces);

	wattrset(pieces, COLOR_PAIR(1));

	box(pieces, 0, 0);
	mvwprintw(pieces, 1, 2, "Next");
	mvwprintw(pieces, 1, 8, "Hold");

	np = HOLD_BLOCK();

	for (i = 0; i < LEN(np->p); i++) {
		wattrset(pieces, A_BOLD | COLOR_PAIR(np->type +1));
		mvwprintw(pieces, np->p[i].y +3,
				np->p[i].x +9, BLOCK_CHAR);
	}

	np = FIRST_NEXT_BLOCK();

	while (np) {
		for (i = 0; i < LEN(np->p); i++) {
			wattrset(pieces, A_BOLD | COLOR_PAIR(np->type +1));
			mvwprintw(pieces, np->p[i].y +3 +(count*3),
					np->p[i].x +3, BLOCK_CHAR);
		}

		count++;
		np = np->entries.le_next;
	}

	wnoutrefresh(pieces);
}

void screen_draw_game(void)
{
	static size_t hash_pieces = 0;
	size_t tmp_hash_pieces;
	size_t i, j;

	/* Center the text horizontally, place the text slightly above
	 * the middle vertically.
	 */
	if (pgame->pause) {
		wattrset(board, COLOR_PAIR(1) | A_BOLD);
		mvwprintw(board, (BLOCKS_MAX_ROWS -6) /2 +GAME_Y_OFF,
				 (BLOCKS_MAX_COLUMNS -2) /2 -1 +GAME_X_OFF,
				 "PAUSED");
		goto done;
	}

	/* It's not possible for a block to appear thrice in a row, so this will
	 * always hash differently if the blocks have changed.
	 */
	tmp_hash_pieces =
		(CURRENT_BLOCK()->type << 16) +
		(FIRST_NEXT_BLOCK()->type << 8) +
		FIRST_NEXT_BLOCK()->entries.le_next->type;

	/* Redraw the blocks if they've changed since last we checked. */
	if (hash_pieces != tmp_hash_pieces) {
		hash_pieces = tmp_hash_pieces;
		screen_draw_blocks();
	}

	/***********************/
	/* Draw the Game board */
	/***********************/

	wattrset(board, COLOR_PAIR(5) | A_BOLD);

	mvwprintw(board, TEXT_Y_OFF+1, TEXT_X_OFF+7, "%7d", pgame->level);
	mvwprintw(board, TEXT_Y_OFF+2, TEXT_X_OFF+7, "%7d", pgame->score);
	mvwprintw(board, TEXT_Y_OFF+3, TEXT_X_OFF+7, "%7d", pgame->pause_ticks);

	wattrset(board, COLOR_PAIR(1));

	/* Draw the background of the board. Dot every other column */
	for (i = 2; i < BLOCKS_MAX_ROWS; i++)
		mvwprintw(board, i -2 +GAME_Y_OFF, 1 +GAME_X_OFF,
				" . . . . .");

	/* Draw the Ghost block on the bottom of the board, if the user wants. */
	/* We draw this before we draw the pieces so that the falling block covers
	 * the outline of the ghost block. */
	if (pgame->ghosts && ghost_block) {
		wattrset(board, A_DIM|COLOR_PAIR(ghost_block->type %sizeof(colors) +1));
		for (i = 0; i < LEN(ghost_block->p); i++) {
			mvwprintw(board,
				ghost_block->p[i].y +GAME_Y_OFF +ghost_block->row_off -2,
				ghost_block->p[i].x +GAME_X_OFF +ghost_block->col_off +1,
				BLOCK_CHAR);
		}
	}

	/* Draw the game board, minus the two hidden rows above the game */
	for (i = 2; i < BLOCKS_MAX_ROWS; i++) {
		for (j = 0; j < BLOCKS_MAX_COLUMNS && pgame->spaces[i]; j++) {
			if (!blocks_at_yx(i, j))
				continue;

			wattrset(board, A_BOLD | COLOR_PAIR(
					(pgame->colors[i][j] %sizeof(colors))
					+1));
			mvwprintw(board, i -2 +GAME_Y_OFF, j +1 +GAME_X_OFF,
					BLOCK_CHAR);
		}
	}

	done:

	wnoutrefresh(board);

	doupdate();
}

/* Game over screen */
void screen_draw_over(void)
{
	log_info("Game over");

	clear();
	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);

	mvprintw(1, 1, "Local Leaderboard");
	mvprintw(2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw(LINES - 2, 1, "Press F1 to quit.");

	if (pgame->lose) {
		db_save_score();
	} else {
		db_save_state();
		return;
	}

	/* Print score board when you lose a game */
	struct db_results *res = db_get_scores(10);
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
