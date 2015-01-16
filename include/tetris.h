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

/* There will always be ghosts in machines - Asimov */

#ifndef TETRIS_H_
#define TETRIS_H_

#include <ncurses.h>

#if defined(WIDE_NCURSES_HEADER)
# include <ncursesw/ncurses.h>
#endif

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/queue.h>
#include <time.h>

/* Game dimensions */
#define TETRIS_MAX_COLUMNS	10
#define TETRIS_MAX_ROWS		22

/* Number of "next" blocks */
#define TETRIS_NEXT_BLOCKS_LEN	5

/* Block types */
#define TETRIS_I_BLOCK		1
#define TETRIS_T_BLOCK		2
#define TETRIS_L_BLOCK		3
#define TETRIS_J_BLOCK		4
#define TETRIS_O_BLOCK		5
#define TETRIS_S_BLOCK		6
#define TETRIS_Z_BLOCK		7

#define TETRIS_NUM_BLOCKS	7

typedef struct tetris tetris;

typedef struct block block;
struct block {
	uint8_t soft_drop, hard_drop;
	bool hold, t_spin;
	uint8_t type;

	uint8_t col_off, row_off;
	struct pieces {
		int8_t x, y;
	} p[4];

	LIST_ENTRY(block) entries;
};

/* Create game state */
int tetris_init(tetris **);

/* Free memory */
int tetris_cleanup(tetris *);

/* Commands */
#define TETRIS_MOVE_LEFT	0x00
#define TETRIS_MOVE_RIGHT	0x01
#define TETRIS_MOVE_DOWN	0x02
#define TETRIS_MOVE_DROP	0x03
#define TETRIS_ROT_LEFT		0x04
#define TETRIS_ROT_RIGHT	0x05
#define TETRIS_HOLD_BLOCK	0x06
#define TETRIS_QUIT_GAME	0x07
#define TETRIS_PAUSE_GAME	0x08
#define TETRIS_GAME_TICK	0x09

/* Process key command in ch and modify game */
int tetris_cmd(tetris *, int command);


/* Set Attributes */
#define TETRIS_SET_GHOSTS	0x10
#define TETRIS_SET_WALLKICKS	0x11
#define TETRIS_SET_TSPIN	0x12
#define TETRIS_SET_NAME		0x13
#define TETRIS_SET_DBFILE	0x14

int tetris_set_attr(tetris *, int attrib, ...);


/* Get Attributes */
#define TETRIS_GET_PAUSED	0x20
#define TETRIS_GET_WIN		0x21
#define TETRIS_GET_LOSE		0x22
#define TETRIS_GET_QUIT		0x23
#define TETRIS_GET_NAME		0x24
#define TETRIS_GET_LEVEL	0x25
#define TETRIS_GET_LINES	0x26
#define TETRIS_GET_SCORE	0x27
#define TETRIS_GET_DELAY	0x28
#define TETRIS_GET_GHOSTS	0x29
#define TETRIS_GET_DBFILE	0x2A

int tetris_get_attr(tetris *, int attrib, ...);


/* HOLD, CURRENT, NEXT(0), NEXT(1), NEXT(2), NEXT(3), NEXT(4) */
#define TETRIS_BLOCK_GHOST	0x30
#define TETRIS_BLOCK_HOLD	0x31
#define TETRIS_BLOCK_CURRENT	0x32
#define TETRIS_BLOCK_NEXT0	0x33
#define TETRIS_BLOCK_NEXT(n)	(TETRIS_BLOCK_NEXT0+(n))

int tetris_get_block(tetris *, int attrib, block *);


/* Database stuff */
TAILQ_HEAD(tetris_score_head, tetris_score);
typedef struct tetris_score tetris_score;
struct tetris_score {
	char id[16];
	uint32_t score;
	uint8_t level;
	time_t date;
	TAILQ_ENTRY(tetris_score) entries;
};

int tetris_save_score(tetris *);
int tetris_save_state(tetris *);
int tetris_resume_state(tetris *);

int tetris_get_scores(tetris *, struct tetris_score_head **res, size_t n);
void tetris_clean_scores(tetris *);


/* Draw functions */
int tetris_draw_board(tetris *, WINDOW *);
int tetris_draw_pieces(tetris *, WINDOW *);
int tetris_draw_text(tetris *, WINDOW *, int width, int height);

/* Game modes, we win when the game mode returns 1 */

int tetris_set_win_condition(tetris *, int (*)(tetris *));

/* Classic tetris, game goes on until we lose */
int tetris_classic(tetris *);

/* TODO */
//int tetris_40_lines(tetris *);
//int tetris_timed(tetris *);

#endif	/* TETRIS_H_ */
