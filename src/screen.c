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

#define BLOCK_CHAR "x"

static WINDOW *board, *control;

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

	board = newwin(BLOCKS_ROWS - 1, BLOCKS_COLUMNS + 2, 1, 18);
	control = newwin(16, 16, 1, 1);

	start_color();
	for (size_t i = 0; i < LEN(colors); i++)
		init_pair(i + 1, colors[i], COLOR_BLACK);

	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);
	refresh();
}

void screen_cleanup(void)
{
	log_info("Cleaning ncurses context");
	delwin(board);
	delwin(control);
	clear();
	endwin();
}

/* Ask user for difficulty and their name */
void screen_draw_menu(struct block_game *pgame, struct db_info *psave)
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
#if defined(DEBUG)
		":memory:");
#else
		"%s/.local/share/tetris/saves", getenv("HOME"));
#endif

	/* Set the board to 'medium' size */
	/* TODO get dimensions from user later */
	pgame->width = 10;
	pgame->height = (pgame->width * 2) + 2;

	/* Start the game paused if we can resume from an old save */
	if (db_resume_state(psave, pgame) > 0) {
		pgame->pause = true;
	}
}

static inline void screen_draw_control(struct block_game *pgame)
{
	wattrset(control, COLOR_PAIR(1));
	mvwprintw(control, 0, 0, "Tetris-" VERSION);
	mvwprintw(control, 2, 1, "Level %d", pgame->level);
	mvwprintw(control, 3, 1, "Score %d", pgame->score);

	mvwprintw(control, 5, 1, "Next   Save");
	mvwprintw(control, 6, 2, "           ");
	mvwprintw(control, 7, 2, "           ");

	mvwprintw(control, 9, 1, "Controls");
	mvwprintw(control, 10, 2, "Pause [F1]");
	mvwprintw(control, 11, 2, "Quit [F3]");
	mvwprintw(control, 13, 2, "Move [asd]");
	mvwprintw(control, 14, 2, "Rotate [qe]");
	mvwprintw(control, 15, 2, "Save [space]");

	for (size_t i = 0; i < LEN(pgame->next->p); i++) {
		char y, x;
		y = pgame->next->p[i].y;
		x = pgame->next->p[i].x;

		wattrset(control, COLOR_PAIR((pgame->next->color
					      % LEN(colors)) + 1) | A_BOLD);
		mvwprintw(control, y + 7, x + 3, BLOCK_CHAR);

	}

	for (size_t i = 0; pgame->save && i < LEN(pgame->save->p); i++) {
		char y, x;
		y = pgame->save->p[i].y;
		x = pgame->save->p[i].x;

		wattrset(control, COLOR_PAIR((pgame->save->color
					      % LEN(colors)) + 1) | A_BOLD);
		mvwprintw(control, y + 7, x + 9, BLOCK_CHAR);
	}

	wrefresh(control);
}

static inline void screen_draw_board(struct block_game *pgame)
{
	wattrset(board, A_BOLD | COLOR_PAIR(5));
	mvwvline(board, 0, 0, '*', pgame->height - 1);

	mvwvline(board, 0, pgame->width + 1, '*', pgame->height - 1);

	mvwhline(board, pgame->height - 2, 0, '*', pgame->width + 2);

	/* Draw the game board, minus the two hidden rows above the game */
	for (int i = 2; i < pgame->height; i++) {
		wmove(board, i - 2, 1);

		for (int j = 0; j < pgame->width; j++) {
			if (blocks_at_yx(pgame, i, j)) {
				wattrset(board, COLOR_PAIR((pgame->colors[i][j]
							    % LEN(colors)) +
							   1) | A_BOLD);
				wprintw(board, BLOCK_CHAR);
			} else {
				wattrset(board, COLOR_PAIR(1));
				wprintw(board, (j % 2) ? "." : " ");
			}
		}
	}

	/* Overlay the PAUSED text */
	wattrset(board, COLOR_PAIR(1) | A_BOLD);
	if (pgame->pause) {
		char y_off, x_off;

		/* Center the text horizontally, place the text slightly above
		 * the middle vertically.
		 */
		x_off = (pgame->width - 6) / 2 + 1;
		y_off = (pgame->height - 2) / 2 - 2;

		mvwprintw(board, y_off, x_off, "PAUSED");
	}

	wrefresh(board);
}

void screen_draw_game(struct block_game *pgame)
{
	screen_draw_control(pgame);
	screen_draw_board(pgame);
}

/* Game over screen */
void screen_draw_over(struct block_game *pgame, struct db_info *psave)
{
	if (!pgame || !psave)
		return;

	log_info("Game over");

	clear();
	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);

	mvprintw(1, 1, "Local Leaderboard");
	mvprintw(2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw(LINES - 2, 1, "Press F1 to quit.");

	if (pgame->loss) {
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
