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

#include <ncurses.h>

#if defined(SEPARATE_WIDE_NCURSES_HEADER)
# include <ncursesw/ncurses.h>
#endif

#define PIECES_CHAR WACS_BLOCK

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "screen.h"
#include "db.h"
#include "conf.h"
#include "logs.h"
#include "tetris.h"
#include "helpers.h"


#define PIECES_Y_OFF	4
#define PIECES_X_OFF	3
#define PIECES_HEIGHT	16
#define PIECES_WIDTH	13

#define BOARD_Y_OFF	1
#define BOARD_X_OFF	18
#define BOARD_HEIGHT	TETRIS_MAX_ROWS
#define BOARD_WIDTH	TETRIS_MAX_COLUMNS +2

#define TEXT_Y_OFF	1
#define TEXT_X_OFF	BOARD_X_OFF + BOARD_WIDTH
#define TEXT_HEIGHT	BOARD_HEIGHT
#define TEXT_WIDTH	(COLS - TEXT_X_OFF -1)

static WINDOW *board, *pieces, *text;
const char colors[] = {
	COLOR_WHITE, COLOR_RED, COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN };

extern struct config *config;

int screen_nc_init(void)
{
	debug("Initializing ncurses context");
	initscr();

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);

	start_color();
	for (size_t i = 0; i < LEN(colors); i++)
		init_pair(i + 1, colors[i], COLOR_BLACK);

	attrset(COLOR_PAIR(1));

	/* Draw static text */
	box(stdscr, 0, 0);
	mvprintw(1, 1, "Tetris-" VERSION);

	mvprintw(PIECES_Y_OFF-1, PIECES_X_OFF+1, "Hold   Next");

	pieces = newwin(PIECES_HEIGHT, PIECES_WIDTH,
			PIECES_Y_OFF, PIECES_X_OFF);

	board = newwin(BOARD_HEIGHT, BOARD_WIDTH,
			BOARD_Y_OFF, BOARD_X_OFF);

	text = newwin(TEXT_HEIGHT, TEXT_WIDTH,
			TEXT_Y_OFF, TEXT_X_OFF);

	refresh();

	return 1;
}

void screen_nc_cleanup(void)
{
	delwin(board);
	delwin(pieces);
	delwin(text);

	clear();
	endwin();
}

int screen_nc_menu(tetris *pgame)
{
	(void) pgame;
	return 1;
}

int screen_nc_update(tetris *pgame)
{
	werase(board);
	werase(pieces);
	werase(text);

	const block *pblock;
	size_t i, count = 0;

	/* draw pieces */
	wattrset(pieces, COLOR_PAIR(1));
	box(pieces, 0, 0);

	pblock = HOLD_BLOCK(pgame);

	for (i = 0; i < LEN(pblock->p); i++) {
		wattrset(pieces, A_BOLD |
			COLOR_PAIR((pblock->type % LEN(colors)) +1));

		mvwadd_wch(pieces, pblock->p[i].y +2,
				pblock->p[i].x +3, PIECES_CHAR);
	}

	pblock = FIRST_NEXT_BLOCK(pgame);

	while (pblock) {
		for (i = 0; i < LEN(pblock->p); i++) {
			wattrset(pieces, A_BOLD |
				COLOR_PAIR((pblock->type % LEN(colors)) +1));

			mvwadd_wch(pieces, pblock->p[i].y +2 +(count*3),
					pblock->p[i].x +9, PIECES_CHAR);
		}

		count++;
		pblock = pblock->entries.le_next;
	}

	/* draw board */
	/* Draw board outline */
	wattrset(board, A_BOLD | COLOR_PAIR(5));

	mvwvline(board, 0, 0, '*', TETRIS_MAX_ROWS -1);
	mvwvline(board, 0, TETRIS_MAX_COLUMNS +1, '*', TETRIS_MAX_ROWS -1);

	mvwhline(board, TETRIS_MAX_ROWS -2, 0, '*', TETRIS_MAX_COLUMNS +2);

	/* Draw the background of the board. Dot every other column */
	wattrset(board, COLOR_PAIR(1));
	for (i = 2; i < TETRIS_MAX_ROWS; i++)
		mvwprintw(board, i -2, 1, " . . . . .");

	/* Draw the ghost block */
	pblock = pgame->ghost_block;
	wattrset(board, A_DIM |
		COLOR_PAIR((pblock->type % LEN(colors)) +1));

	for (i = 0; i < LEN(pblock->p); i++) {
		mvwadd_wch(board,
		    pblock->p[i].y +pblock->row_off -2,
		    pblock->p[i].x +pblock->col_off +1,
		    PIECES_CHAR);
	}

	/* Draw the game board, minus the two hidden rows above the game */
	for (i = 2; i < TETRIS_MAX_ROWS; i++) {
		if (pgame->spaces[i] == 0)
			continue;

		size_t j;
		for (j = 0; j < TETRIS_MAX_COLUMNS; j++) {
			if (!tetris_at_yx(pgame, i, j))
				continue;

			wattrset(board, A_BOLD |
				COLOR_PAIR(pgame->colors[i][j] +1));
			mvwadd_wch(board, i -2, j +1, PIECES_CHAR);
		}
	}

	/* Draw the falling block to the board */
	pblock = CURRENT_BLOCK(pgame);
	for (i = 0; i < LEN(pblock->p); i++) {
		wattrset(board, A_DIM |
			COLOR_PAIR((pblock->type % LEN(colors)) +1));

		mvwadd_wch(board,
		    pblock->p[i].y +pblock->row_off -2,
		    pblock->p[i].x +pblock->col_off +1,
		    PIECES_CHAR);
	}

	/* Draw "PAUSED" text */
	if (pgame->paused) {
		wattrset(board, A_BOLD | COLOR_PAIR(1));
		mvwprintw(board, (TETRIS_MAX_ROWS -2) /2 -1,
				 (TETRIS_MAX_COLUMNS -2) /2 -1,
				 "PAUSED");
	}

	/* Draw username */
	wattrset(board, COLOR_PAIR(1));
	mvwprintw(board, BOARD_HEIGHT -1, (BOARD_WIDTH -strlen(pgame->id))/2,
		"%s", pgame->id);

	/* Draw text */
	wattrset(text, COLOR_PAIR(1));

	mvwprintw(text, 0, 1, "Gamemode: %s", pgame->gamemode);
	mvwprintw(text, 2, 2, "Level %7d", tetris_get_level(pgame));
	mvwprintw(text, 3, 2, "Score %7d", tetris_get_score(pgame));
	mvwprintw(text, 4, 2, "Lines %7d", tetris_get_lines(pgame));

	mvwprintw(text, 6, 2, "Controls");
	mvwprintw(text, 7 , 3, "Pause: %c", config->pause_key.key);
	mvwprintw(text, 8 , 3, "Save/Quit: %c", config->quit_key.key);
	mvwprintw(text, 9 , 3, "Move: %c%c%c%c",
			config->move_left.key,
			config->move_down.key,
			config->move_right.key,
			config->move_drop.key);

	mvwprintw(text, 10 , 3, "Rotate: %c%c",
			config->rotate_left.key,
			config->rotate_right.key);

	mvwprintw(text, 11, 3, "Hold: %c", config->hold_key.key);
	mvwprintw(text, 13, 1, "--------");

	/* Print in-game logs/messages */
	wattrset(text, COLOR_PAIR(3));

	struct log_entry *lep, *tmp;
	i = 0;

	LIST_FOREACH(lep, &entry_head, entries) {
		/* Display messages, then remove anything that can't fit on
		 * screen
		 */
		if (i < TEXT_HEIGHT-15) {
			mvwprintw(text, 14+i, 2, "%.*s", TEXT_WIDTH -3, lep->msg);
		} else {
			tmp = lep;

			LIST_REMOVE(tmp, entries);
			free(tmp->msg);
			free(tmp);
		}
		i++;
	}

	wnoutrefresh(pieces);
	wnoutrefresh(board);
	wnoutrefresh(text);
	doupdate();

	return 1;
}

/* Game over screen */
int screen_nc_gameover(tetris *pgame)
{
	debug("Drawing game over screen");

	clear();
	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);

	mvprintw(1, 1, "Local Leaderboard");
	mvprintw(2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw(LINES -2, 1, "Press any RETURN to continue ..");

	/* Print score board when you lose a game */
	tetris *res[10];
	if (db_get_scores(pgame, res, LEN(res)) != 1) {
		return 0;
	}

	for (size_t i = 0; i < LEN(res) && res[i]; i++)
	{
		char *date_str = ctime(&((res[i])->date));

		/* Bold the entry we've just added to the highscores */
		if ((tetris_get_score(pgame) == (res[i])->score) &&
		    (tetris_get_level(pgame) == (res[i])->level))
			attrset(COLOR_PAIR(1) | A_BOLD);
		else
			attrset(COLOR_PAIR(1));

		mvprintw(i +3, 4, "%2d.\t%-16s%-5d\t%-5d\t%.*s", i+1,
			 (res[i])->id ? (res[i])->id : "unknown",
			 (res[i])->level, (res[i])->score,
			 strlen(date_str) -1, date_str);
	}

	db_clean_scores(res, LEN(res));

	refresh();
	sleep(1);
	while (getch() != KEY_ENTER);

	return 1;
}
