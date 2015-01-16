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
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "conf.h"
#include "logs.h"
#include "tetris.h"
#include "screen.h"
#include "helpers.h"

static WINDOW *board, *pieces, *text;
const char *colors;

int screen_init(void)
{
	debug("Initializing ncurses context");
	initscr();

	cbreak();
	noecho();
	keypad(stdscr, TRUE);
	curs_set(0);

	colors = malloc(sizeof *colors * COLORS_LENGTH);
	if (!colors) {
		log_err("Out of memory");
		return -1;
	}

	memcpy((char *)colors, ((const char[]) {COLOR_WHITE, COLOR_RED,
		COLOR_GREEN, COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA,
		COLOR_CYAN }), COLORS_LENGTH * sizeof *colors);

	start_color();
	for (size_t i = 0; i < COLORS_LENGTH; i++)
		init_pair(i + 1, colors[i], COLOR_BLACK);

	attrset(COLOR_PAIR(1));

	/* Draw static text */
	box(stdscr, 0, 0);
	mvprintw(1, 1, "Tetris-" VERSION);

	mvprintw(PIECES_Y_OFF-1, PIECES_X_OFF+1, "Hold   Next");

	pieces = newwin(16, 13, PIECES_Y_OFF, PIECES_X_OFF);

	board = newwin(BOARD_HEIGHT, BOARD_WIDTH, BOARD_Y_OFF, BOARD_X_OFF);

	text = newwin(BOARD_HEIGHT, 40, TEXT_Y_OFF, TEXT_X_OFF);

	refresh();

	atexit(screen_cleanup);

	return 1;
}

void screen_cleanup(void)
{
	debug("Cleaning ncurses context");

	delwin(board);
	delwin(pieces);
	delwin(text);

	free((char *)colors);

	clear();
	endwin();
}

void screen_draw_game(tetris *pgame)
{
	tetris_draw_pieces(pgame, pieces);
	tetris_draw_board(pgame, board);
	tetris_draw_text(pgame, text);

	doupdate();
}

/* Game over screen */
void screen_draw_over(tetris *pgame)
{
	debug("Drawing game over screen");

	clear();
	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);

	mvprintw(1, 1, "Local Leaderboard");
	mvprintw(2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw(LINES -2, 1, "Press any key to continue ..");

	/* Print score board when you lose a game */
	struct tetris_score_head *res = NULL;

	if (tetris_get_scores(pgame, &res, 10) != 1) {
		tetris_clean_scores(pgame);
		return;
	}

	tetris_score *pscore;
	pscore = res->tqh_first;

	int count = 1;
	while (pscore) {
		char *date_str = ctime(&pscore->date);

		mvprintw(count +2, 4, "%2d.\t%-16s%-5d\t%-5d\t%.*s", count,
			 pscore->id ? pscore->id : "unknown",
			 pscore->level, pscore->score,
			 strlen(date_str) -1, date_str);

		count++;
		pscore = pscore->entries.tqe_next;
	}

	tetris_clean_scores(pgame);

	refresh();
	while (getch());
}
