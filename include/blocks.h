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

#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#define PI 3.141592653589L
#define LEN(x) ((sizeof(x))/(sizeof(*x)))

/* Define index yourself. We increment here at each definition */
#define PIECE_XY(X, Y) \
	block->p[index].x = (X); block->p[index].y = (Y); index++;

#define BLOCKS_MAX_COLUMNS	10
#define BLOCKS_MAX_ROWS		22

enum blocks_block_types {
	O_BLOCK = 0,			/* square */
	I_BLOCK,			/* line */
	T_BLOCK,
	L_BLOCK,
	J_BLOCK,
	Z_BLOCK,
	S_BLOCK,
};
#define NUM_BLOCKS 7

enum blocks_input_cmd {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_DOWN,			/* Move down one block */
	MOVE_DROP,			/* Drop block to bottom of board */
	ROT_LEFT,
	ROT_RIGHT,
	HOLD,
};

#define NEXT_BLOCKS_LEN 5

/* Only the currently falling block, the next block, and the save block are
 * stored in this structure. Once a block hits another piece, we forget about
 * it; it becomes part of the game board.
 */
struct blocks {
	uint32_t lock_delay;		/* how long to wait (nsec) */
	uint8_t soft_drop, hard_drop;	/* number of blocks dropped */
	uint8_t col_off, row_off;	/* column/row offsets */
	uint8_t color;

	bool t_spin;			/* T-Spin . not implemented*/
	bool hold;			/* can only hold once */

	enum blocks_block_types type;
	struct {			/* pieces stores a value */
		int8_t x, y;		/* between -1 and +2 */
	} p[4];

	LIST_ENTRY(blocks) entries;
};

struct blocks_game {
	/* These variables are read/written to the database
	 * when restoring/saving the game state
	 */
	uint8_t width, height;
	uint16_t level, lines_destroyed;
	uint16_t spaces[BLOCKS_MAX_ROWS];	/* bit-field, one per row */
	uint32_t score;

	uint8_t *colors[BLOCKS_MAX_ROWS];	/* 1-to-1 with board */
	uint32_t nsec;				/* tick delay in nanoseconds */
	uint32_t pause_ticks;			/* total pause ticks per game */
	bool pause;				/* game pause */
	bool lose, quit;			/* how we quit */
	pthread_mutex_t lock;

	LIST_HEAD(blocks_head, blocks) blocks_head;
};

#define HOLD_BLOCK(name) ((name)->blocks_head.lh_first)
#define CURRENT_BLOCK(name) ((name)->blocks_head.lh_first->entries.le_next)
#define FIRST_NEXT_BLOCK(name) ((name)->blocks_head.lh_first->entries.le_next->entries.le_next)

/* Does a block exist at the specified (y, x) coordinate? */
#define blocks_at_yx(p, y, x) ((p)->spaces[(y)] & (1 << (x)))

/* Create game state */
int blocks_init(struct blocks_game *);

/* Free memory */
int blocks_cleanup(struct blocks_game *);

/* Main loop, doesn't return until game is over */
void *blocks_loop(void *);

/* Input loop */
void *blocks_input(void *);

#endif				/* BLOCKS_H_ */
