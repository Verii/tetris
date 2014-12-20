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
#include <ncursesw/ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "db.h"
#include "conf.h"
#include "logs.h"
#include "blocks.h"
#include "screen.h"

/* Hold/Next Pieces are draw to the left of the Game */
#define PIECES_Y_OFF 4
#define PIECES_X_OFF 3
#define PIECES_CHAR WACS_BLOCK

#define BOARD_Y_OFF 1
#define BOARD_X_OFF 18

#define BOARD_HEIGHT BLOCKS_MAX_ROWS
#define BOARD_WIDTH BLOCKS_MAX_COLUMNS +2

/* Text is draw to the right of the game board */
#define TEXT_Y_OFF 1
#define TEXT_X_OFF BOARD_X_OFF + BOARD_WIDTH

static WINDOW *board, *board_opp, *pieces, *text;

static const char colors[] = { COLOR_WHITE, COLOR_RED, COLOR_GREEN,
	COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN
};

int screen_init(void)
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

	pieces = newwin(16, 13, PIECES_Y_OFF, PIECES_X_OFF);

	board = newwin(BOARD_HEIGHT, BOARD_WIDTH, BOARD_Y_OFF, BOARD_X_OFF);

	text = newwin(BOARD_HEIGHT, 40, TEXT_Y_OFF, TEXT_X_OFF);

	board_opp = text;

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

	clear();
	endwin();
}

static void draw_text(struct blocks_game *pgame)
{
	size_t i = 0;

	werase(text);

	wattrset(text, COLOR_PAIR(1));

	mvwprintw(text, 1, 1, "Level");
	mvwprintw(text, 2, 1, "Score");

	mvwprintw(text, 5, 1, "Controls");

	const struct config *conf = conf_get_globals_s();
	mvwprintw(text, 6 , 2, "Pause: ");
	wprintw(text, "%c", conf->pause_key.key);

	mvwprintw(text, 7 , 2, "Save/Quit: ");
	wprintw(text, "%c", conf->quit_key.key);

	mvwprintw(text, 8 , 2, "Move: ");
	wprintw(text, "%c%c%c%c",
			conf->move_left.key,
			conf->move_down.key,
			conf->move_right.key,
			conf->move_drop.key);

	mvwprintw(text, 9 , 2, "Rotate: ");
	wprintw(text, "%c%c",
			conf->rotate_left.key,
			conf->rotate_right.key);

	mvwprintw(text, 10, 2, "Ghosts: ");
	wprintw(text, "%c", conf->toggle_ghosts.key);

	mvwprintw(text, 11, 2, "Hold: ");
	wprintw(text, "%c", conf->hold_key.key);

	wattrset(text, COLOR_PAIR(2));
	mvwprintw(text, 13, 1, "Log:");

	wattrset(text, COLOR_PAIR(5));
	mvwprintw(text, 1, 7, "%7d", pgame->level);
	mvwprintw(text, 2, 7, "%7d", pgame->score);

	/* Print in-game logs/messages */
	wattrset(text, COLOR_PAIR(3));

	struct log_entry *np;
	LIST_FOREACH(np, &entry_head, entries) {
		/* Display 7 messages, then remove anything left over */
		if (i < 7) {
			mvwprintw(text, 14+i, 2, "%s", np->msg);
		} else {
			struct log_entry *tmp = np;

			LIST_REMOVE(tmp, entries);
			free(tmp->msg);
			free(tmp);
		}
		i++;
	}

#ifdef DEBUG
	wattrset(text, A_BOLD | COLOR_PAIR(2));
	mvwprintw(text, 19, 1, "DEBUG @ %s %s", __DATE__, __TIME__);

	wattrset(text, A_BOLD | COLOR_PAIR(3));
	mvwprintw(text, 20, 1, "saving to in-memory database");
#endif

	wnoutrefresh(text);
}

/* Draw the Hold and Next blocks listing on the side of the game. */
static void draw_pieces(struct blocks_game *pgame)
{
	struct blocks *np;
	size_t i, count = 0;

	werase(pieces);

	wattrset(pieces, COLOR_PAIR(1));

	box(pieces, 0, 0);
	np = HOLD_BLOCK(pgame);

	for (i = 0; i < LEN(np->p); i++) {
		wattrset(pieces, A_BOLD | COLOR_PAIR(np->type +1));
		mvwadd_wch(pieces, np->p[i].y +2,
				np->p[i].x +3, PIECES_CHAR);
	}

	np = FIRST_NEXT_BLOCK(pgame);

	while (np) {
		for (i = 0; i < LEN(np->p); i++) {
			wattrset(pieces, A_BOLD | COLOR_PAIR(np->type +1));
			mvwadd_wch(pieces, np->p[i].y +2 +(count*3),
					np->p[i].x +9, PIECES_CHAR);
		}

		count++;
		np = np->entries.le_next;
	}

	wnoutrefresh(pieces);
}

static void draw_board(bool self, struct blocks_game *pgame, WINDOW *win)
{
	size_t i, j, x_off;

	char player_str[10];

	if (self) {
		strncpy(player_str, pgame->id, sizeof player_str);
		x_off = 0;
	} else {
#if 0
		strncpy(player_str,
			pgame->opp->id ? pgame->opp->id : "Player 2",
			sizeof player_str);
#endif
		x_off = 16;
	}

	werase(win);

	/* Draw board outline */
	wattrset(win, A_BOLD | COLOR_PAIR(5));

	mvwvline(win, 0, x_off, '*', BLOCKS_MAX_ROWS -1);
	mvwvline(win, 0, x_off +BLOCKS_MAX_COLUMNS +1, '*', BLOCKS_MAX_ROWS -1);

	mvwhline(win, BLOCKS_MAX_ROWS -2, x_off, '*', BLOCKS_MAX_COLUMNS +2);

	/* Draw the background of the board. Dot every other column */
	wattrset(win, COLOR_PAIR(1));
	for (i = 2; i < BLOCKS_MAX_ROWS; i++)
		mvwprintw(win, i -2, x_off +1, " . . . . .");

	/* Draw the Ghost block on the bottom of the board, if the user wants */
	if (pgame->ghosts && pgame->ghost) {
		struct blocks *np = pgame->ghost;

		wattrset(win, A_DIM| COLOR_PAIR(np->type %sizeof(colors) +1));

		for (i = 0; i < LEN(np->p); i++) {
			mvwadd_wch(win,
			    np->p[i].y +np->row_off -2,
			    np->p[i].x +np->col_off +1 +x_off,
			    PIECES_CHAR);
		}
	}

	/* Draw the game board, minus the two hidden rows above the game */
	for (i = 2; i < BLOCKS_MAX_ROWS; i++) {
		if (pgame->spaces[i] == 0)
			continue;

		for (j = 0; j < BLOCKS_MAX_COLUMNS; j++) {
			if (!blocks_at_yx(pgame, i, j))
				continue;

			wattrset(win, A_BOLD | COLOR_PAIR(
					(pgame->colors[i][j] %sizeof(colors))
					+1));
			mvwadd_wch(win, i -2, j +1 +x_off, PIECES_CHAR);
		}
	}

	/* Draw the falling block to the board */
	for (i = 0; i < LEN(CURRENT_BLOCK(pgame)->p); i++) {
		struct blocks *np = CURRENT_BLOCK(pgame);

		wattrset(win, A_DIM| COLOR_PAIR(np->type %sizeof(colors) +1));

		mvwadd_wch(win,
		    np->p[i].y +np->row_off -2,
		    np->p[i].x +np->col_off +1 +x_off,
		    PIECES_CHAR);
	}

	/* Center the text horizontally, place the text slightly above
	 * the middle vertically.
	 */
	if (pgame->pause) {
		wattrset(win, A_BOLD | COLOR_PAIR(1));
		mvwprintw(win, (BLOCKS_MAX_ROWS -2) /2 -1,
				 (BLOCKS_MAX_COLUMNS -2) /2 -1 +x_off,
				 "PAUSED");
	}

	int player_len = strlen(player_str);

	wattrset(win, COLOR_PAIR(1));
	mvwprintw(win, BOARD_HEIGHT -1, (BOARD_WIDTH -player_len)/2 +x_off,
			"%s", player_str);

	if (self == false) {
		const char *brick_wall[] = {
			"_|___|___|___|___|___|___|___|",
			"___|___|___|___|___|___|___|__",
		};

		wattrset(win, COLOR_PAIR(2));
		for (i = 0; i < BOARD_HEIGHT -2; i++)
			mvwprintw(win, i, 0, "%.*s", x_off, brick_wall[i % 2]);

		wattrset(win, A_BOLD | COLOR_PAIR(5));
		mvwprintw(win, BOARD_HEIGHT -1, (x_off -6) /2, "%s", "versus");
	}

	wnoutrefresh(win);
}

void screen_draw_game(struct blocks_game *pgame)
{
	static size_t hash_pieces = 0;

	/* It's not possible for a block to appear thrice in a row, so this will
	 * always hash differently if the blocks have changed.
	 */
	size_t tmp_hash_pieces =
		(CURRENT_BLOCK(pgame)->type << 16) +
		(FIRST_NEXT_BLOCK(pgame)->type << 8) +
		(FIRST_NEXT_BLOCK(pgame)->entries.le_next->type);

	/* Redraw the blocks if they've changed since last we checked. */
	if (hash_pieces != tmp_hash_pieces) {
		hash_pieces = tmp_hash_pieces;
		draw_pieces(pgame);
	}

	/* Draw "Our" board. */
	draw_board(true, pgame, board);
	draw_text(pgame);

	doupdate();
}

/* Game over screen */
void screen_draw_over(void)
{
	debug("Drawing game over screen");

	clear();
	attrset(COLOR_PAIR(1));
	box(stdscr, 0, 0);

	mvprintw(1, 1, "Local Leaderboard");
	mvprintw(2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw(LINES -2, 1, "Press Space to quit.");

	/* Print score board when you lose a game */
	struct db_results *res = NULL;

	if (db_get_scores(&res, 10) != 1) {
		db_clean_scores();
		return;
	}

	while (res) {
		char *date = ctime(&res->date);
		static unsigned char count = 0;

		count++;
		mvprintw(count +2, 4, "%2d.\t%-16s%-5d\t%-5d\t%.*s", count,
			 res->id ? res->id : "unknown",
			 res->level, res->score,
			 strlen(date) -1, date);

		res = res->entries.tqe_next;
	}

	db_clean_scores();

	refresh();
	while (getch() != ' ') ;
}
