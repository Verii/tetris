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

#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#include "helpers.h"

/* Forward declarations, because we have a definition dependency */
struct blocks;
struct blocks_game;
enum blocks_block_types;
enum blocks_input_cmd;

#define BLOCKS_MAX_COLUMNS	10
#define BLOCKS_MAX_ROWS		22

#define NUM_BLOCKS		7
#define NEXT_BLOCKS_LEN		5

/* First, Second, and Third elements in the linked list */
#define HOLD_BLOCK(G) ((G)->blocks_head.lh_first)
#define CURRENT_BLOCK(G) (HOLD_BLOCK((G))->entries.le_next)
#define FIRST_NEXT_BLOCK(G) (CURRENT_BLOCK((G))->entries.le_next)

/* Does a block exist at the specified (y, x) coordinate? */
#define blocks_at_yx(G, Y, X) (G->spaces[(Y)] & (1 << (X)))

/* Returns 1 if we've won, and sets win field to true. */
typedef int (*blocks_win_condition)(struct blocks_game *);

enum blocks_block_types {
	O_BLOCK,
	I_BLOCK,
	T_BLOCK,
	L_BLOCK,
	J_BLOCK,
	Z_BLOCK,
	S_BLOCK,
};

enum blocks_input_cmd {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_DOWN,			/* Move down one block */
	MOVE_DROP,			/* Drop block to bottom of board */
	ROT_LEFT,
	ROT_RIGHT,
	HOLD_PIECE,
	SAVE_QUIT,
	PAUSE_GAME,

	/* Game attributes */
	TOGGLE_GHOSTS,
	TOGGLE_HOLD,
	TOGGLE_PAUSE,
	TOGGLE_WALLKICKS,
	TOGGLE_TSPIN,
};

/* Only 7 blocks(cur, hold, 5 next blocks) are stored in
 * this structure. Once a block hits another piece, we forget about it; it
 * becomes part of the game board.
 */
struct blocks {
	uint32_t lock_delay;		/* how long to wait (nsec) */
	uint8_t soft_drop, hard_drop;	/* number of rows dropped */

	bool t_spin;			/* T-Spin */
	bool hold;			/* can only hold once */

	enum blocks_block_types type;

	uint8_t col_off, row_off;	/* column/row offsets */
	struct pieces {			/* pieces stores two values(x, y) */
		int8_t x, y;		/* between -1 and +2 */
	} p[4];				/* each block has 4 pieces */

	LIST_ENTRY(blocks) entries;	/* LL entries */
};

struct blocks_game {
	/* These variables are read/written to the database
	 * when restoring/saving the game state
	 */
	char id[16];
	uint16_t level, lines_destroyed;
	uint16_t spaces[BLOCKS_MAX_ROWS];	/* bit-field, one per row */
	uint32_t score;

	uint8_t *colors[BLOCKS_MAX_ROWS];	/* 1-to-1 with board */
	uint32_t nsec;			/* tick delay in nanoseconds */
	bool paused;			/* game pause? */
	bool lose, quit, win;		/* quit? lose? win? */

	LIST_HEAD(blocks_head, blocks) blocks_head;	/* list of blocks */
	blocks_win_condition	check_win;

	struct blocks		*ghost;

	/* Attributes */
	bool allow_ghosts;
	bool allow_hold;
	bool allow_pause;
	bool allow_wallkicks;
	bool allow_tspin;
};

/* event loop checks if this variable is set, then calls tick function */
volatile sig_atomic_t blocks_do_update;

/* Functions to modify the currently falling block:
 * translate, rotate, wall kicks, gravity(userinput, real)
 */
int blocks_translate(struct blocks_game *, struct blocks *, enum blocks_input_cmd);
int blocks_rotate(struct blocks_game *, struct blocks *, enum blocks_input_cmd);
int blocks_wall_kick(struct blocks_game *, struct blocks *, enum blocks_input_cmd);
int blocks_fall(struct blocks_game *, struct blocks *);

/* Called if translate(), rotate(), or wall_kick() suceeds */
void blocks_update_ghost_block(struct blocks_game *, struct blocks *);

/* Update tick speed reduces time between gravity events.
 * Update cur block removes the currently falling block from the linked list
 * 	and sets the current block to the next block in the list.
 * Update ghost block copies the memory from the current block, then moves it
 * 	to the bottom of the game.
 * Write current block writes the block to the board.
 */
void blocks_update_tick_speed(struct blocks_game *);
void blocks_update_cur_block(struct blocks_game *);
void blocks_write_block(struct blocks_game *, struct blocks *);

int blocks_set_win_condition(struct blocks_game *, blocks_win_condition);

/* destroy lines is called after a block hits the bottom. It returns the number
 * of lines destroyed [0-4].
 * update points gives points to the game, it accepts the number of lines
 * destroyed as its argument. (fails if lines is not between [1-4]).
 */
int blocks_destroy_lines(struct blocks_game *);
void blocks_update_points(struct blocks_game *, uint8_t);

/* Create game state */
int blocks_init(struct blocks_game **);

/* Free memory */
int blocks_cleanup(struct blocks_game *);

/* Control gravity, etc. */
void blocks_tick(struct blocks_game *);

/* Process key command in ch and modify game */
int blocks_input(struct blocks_game *, enum blocks_input_cmd);


/* Game modes, we win when the game mode returns 1 */

/* Classic tetris, game goes on until we lose */
int blocks_never_win(struct blocks_game *);

/* TODO Get the highest score with only 40 lines */
int blocks_40_lines(struct blocks_game *);

#endif				/* BLOCKS_H_ */
