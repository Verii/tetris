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

#include <ctype.h>
#include <math.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bag.h"
#include "blocks.h"
#include "debug.h"
#include "screen.h"

/*
 * randomizes block and sets the initial positions of the pieces
 */
static void randomize_block(struct blocks_game *pgame, struct blocks *block)
{
	int index = 0; /* used in macro def. PIECE_XY() */

	if (!pgame || !block)
		return;

	memset(block, 0, sizeof *block);

	if (bag_is_empty())
		bag_random_generator();

	block->type = bag_next_piece();

	/* Try to center the block on the board */
	block->col_off = pgame->width / 2;
	block->row_off = 1;

	/* Get a consistent color for each block. We don't know what it is.
	 * screen.c decides in what order colors are defined */
	block->color = block->type;

	/* The piece at (0, 0) is the pivot when we rotate */
	switch (block->type) {
	case O_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		break;
	case I_BLOCK:
		block->col_off--;	/* center */

		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		PIECE_XY( 2,  0);
		break;
	case T_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case L_BLOCK:
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case J_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case Z_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case S_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		break;
	}
}

/*
 * To prevent an additional free/malloc we recycle the allocated memory.
 *
 * First remove it from its current state, causing the next block to 'fall'
 * into place. Then we randomize it(WHICH WIPES ALL DATA, INCLUDING PREVIOUS
 * POINTERS) and reinstall it at the end of the list.
 */
static void update_cur_block(struct blocks_game *pgame)
{
	struct blocks *last, *np = CURRENT_BLOCK(pgame);

	LIST_REMOVE(CURRENT_BLOCK(pgame), entries);

	randomize_block(pgame, np);

	/* Find last block in list */
	for (last = HOLD_BLOCK(pgame);
	     last->entries.le_next;
	     last = last->entries.le_next)
		;

	LIST_INSERT_AFTER(last, np, entries);
}

/* rotate pieces in blocks by either 90^ or -90^ around (0, 0) pivot */
static int rotate_block(struct blocks_game *pgame, struct blocks *block,
			enum blocks_input_cmd cmd)
{
	int new_x, new_y;
	int bounds_x, bounds_y;
	int dir = 1;
	size_t i;

	if (!pgame || !block)
		return -1;

	/* Don't rotate O block */
	if (block->type == O_BLOCK)
		return 1;

	if (cmd == ROT_LEFT)
		dir = -1;

	/* Check each piece for a collision before we write any changes */
	for (i = 0; i < LEN(block->p); i++) {
		bounds_x = block->p[i].y * (-dir) + block->col_off;
		bounds_y = block->p[i].x * (dir) + block->row_off;

		/* Check for out of bounds on each piece */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    /* Also check if a piece already exists here */
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	/* No collisions, so update the block position. */
	for (i = 0; i < LEN(block->p); i++) {
		new_x = block->p[i].y * (-dir);
		new_y = block->p[i].x * (dir);

		block->p[i].x = new_x;
		block->p[i].y = new_y;
	}

	return 1;
}

/* translate pieces in block horizontally. */
static int translate_block(struct blocks_game *pgame, struct blocks *block,
			   enum blocks_input_cmd cmd)
{
	int bounds_x, bounds_y;
	int dir = 1;
	size_t i;

	if (!pgame || !block)
		return -1;

	if (cmd == MOVE_LEFT)
		dir = -1;

	/* Check each piece for a collision */
	for (i = 0; i < LEN(block->p); i++) {
		bounds_x = block->p[i].x + block->col_off + dir;
		bounds_y = block->p[i].y + block->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	block->col_off += dir;

	return 1;
}

/*
 * Tetris Guidlines say wallkicks first try to move left, attempt rotation
 * again. Then if that fails, we try again but by moving to the right.
 */
static int try_wall_kick(struct blocks_game *pgame, struct blocks *block,
			enum blocks_input_cmd cmd)
{
	/* Try to move left and rotate again. */
	if (translate_block(pgame, block, MOVE_LEFT) == 1) {
		if (rotate_block(pgame, block, cmd) == 1)
			return 1;
	}

	/* undo previous translation */
	translate_block(pgame, block, MOVE_RIGHT);

	/* Try to move right and rotate again. */
	if (translate_block(pgame, block, MOVE_RIGHT) == 1) {
		if (rotate_block(pgame, block, cmd) == 1)
			return 1;
	}

	return 0;
}

/*
 * Decrease the tick delay of the falling block.
 * Algorithm will most likely change. It currently follows the arctan curve.
 * Not quite sure that I like it, though.
 */
static void update_tick_speed(struct blocks_game *pgame)
{
	double speed = 1.0f;

	if (!pgame)
		return;

	/* See tests/level-curve.c */
	speed += atan((double)pgame->level / 5.0) * 2 / PI * 3;
	pgame->nsec = (uint32_t) ((double)1E9 / speed) - 1;
}

/*
 * We first check each row for a full line, and shift all rows above this down.
 * We currently implement naive gravity.
 *
 * We do a bit of logic to add points to the game. Line clears which are
 * considered difficult(as per the Tetris Guidlines) will yield more points by
 * a factor of 3/2 during consecutive moves.
 *
 * We also update the level here. Which currently only affects the speed of the
 * falling blocks, and counts as a multiplier for points(i.e. level 2 will
 * yield (points *2)). The level increments when we destroy (level *2) +2 lines.
 */
static int destroy_lines(struct blocks_game *pgame)
{
	uint32_t full_row;
	uint8_t destroyed = 0;
	size_t i, j;

	/* point modifier, "difficult" line clears earn more over time.
	 * a tetris (4 line clears) counts for 1 difficult move.
	 *
	 * difficult values >1 boost points by 3/2
	 */
	static size_t difficult = 0;
	uint32_t point_mod = 0;

	if (!pgame)
		return -1;

	/* Fill in all bits below bit pgame->width. Row populations are stored
	 * in a bit field, so we can check for a full row by comparing it to
	 * this value.
	 */
	full_row = (1 << pgame->width) -1;

	/* The first two rows are 'above' the game, that's where the new blocks
	 * come into existence. We lose if there's ever a block there. */
	for (i = 0; i < 2; i++)
		if (pgame->spaces[i])
			pgame->lose = true;

	/* Check each row for a full line,
	 * we check in reverse to ease moving the rows down when we find a full
	 * row. Ignore the top two rows, which we checked above.
	 */
	for (i = pgame->height - 1; i >= 2; i--) {

		if (pgame->spaces[i] != full_row)
			continue;

		/* Move lines above destroyed line down */
		for (j = i; j > 0; j--)
			pgame->spaces[j] = pgame->spaces[j - 1];

		/* Lines are moved down, so we recheck this row. */
		i++;
		destroyed++;
	}

	pgame->lines_destroyed += destroyed;
	if (pgame->lines_destroyed >= (pgame->level * 2 + 2)) {
		pgame->lines_destroyed -= (pgame->level * 2 + 2);
		pgame->level++;
		update_tick_speed(pgame);
	}

	/* We lose our difficulty multipliers on easy moves */
	if ((destroyed && destroyed != 4) && !CURRENT_BLOCK(pgame)->t_spin)
		difficult = 0;

	if (CURRENT_BLOCK(pgame)->t_spin)
		difficult++;

	/* Number of lines destroyed in move */
	switch (destroyed) {
		case 1:
			point_mod = 100;
			break;
		case 2:
			point_mod = 300;
			break;
		case 3:
			point_mod = 500;
			break;
		case 4:
			difficult++;
			point_mod = (difficult > 1) ? 1200 : 800;
			break;
	}

	pgame->score += point_mod * pgame->level
		+ CURRENT_BLOCK(pgame)->soft_drop
		+ (CURRENT_BLOCK(pgame)->hard_drop * 2);

	return destroyed;
}

/*
 * Remove the currently falling block from the board.
 * NOT THE LINKED LIST. The block still exists in memory. We are literally just
 * erasing the bits from the actual game board. This is used before operating
 * on a game piece(e.g. before rotation or translation).
 */
static void unwrite_cur_block(struct blocks_game *pgame)
{
	struct blocks *block;
	size_t i, x, y;

	if (!pgame || !CURRENT_BLOCK(pgame))
		return;

	block = CURRENT_BLOCK(pgame);

	for (i = 0; i < LEN(block->p); i++) {
		y = block->row_off + block->p[i].y;
		x = block->col_off + block->p[i].x;

		/* Remove the bit where the block exists */
		pgame->spaces[y] &= ~(1 << x);
	}
}

/*
 * Inverse of the above. We write the pieces to the game board.
 * Used after operations on a block(e.g. rotation or translation).
 */
static void write_cur_block(struct blocks_game *pgame)
{
	struct blocks *block;
	int px[4], py[4];
	size_t i;

	if (!pgame || !CURRENT_BLOCK(pgame))
		return;

	block = CURRENT_BLOCK(pgame);

	for (i = 0; i < LEN(block->p); i++) {
		py[i] = block->row_off + block->p[i].y;
		px[i] = block->col_off + block->p[i].x;

		if (px[i] < 0 || px[i] >= pgame->width ||
		    py[i] < 0 || py[i] >= pgame->height)
			return;
	}

	/* pgame->spaces is an array of bit fields, 1 per row */
	for (i = 0; i < LEN(block->p); i++) {
		/* Set the bit where the block exists */
		pgame->spaces[py[i]] |= (1 << px[i]);
		pgame->colors[py[i]][px[i]] = block->color;
	}
}

/*
 * Attempts to translate the block one row down.
 * This function is used during normal gravitational events, and during
 * user-input 'soft drop' events.
 */
static int drop_block(struct blocks_game *pgame, struct blocks *block)
{
	size_t i, bounds_x, bounds_y;

	if (!pgame || !block)
		return -1;

	for (i = 0; i < LEN(block->p); i++) {
		bounds_y = block->p[i].y + block->row_off + 1;
		bounds_x = block->p[i].x + block->col_off;

		if (bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	block->row_off++;

	return 1;
}

/*
 * Setup the game structure for use.
 * Here we create the initial game pieces for the game (5 'next' pieces, plus
 * the current piece and the 'hold' piece(total 7 game pieces).
 * We also allocate memory for the board colors, and set some initial
 * variables.
 */
int blocks_init(struct blocks_game *pgame)
{
	size_t i;

	if (!pgame)
		return -1;

	log_info("Initializing game data");
	memset(pgame, 0, sizeof *pgame);

	pthread_mutex_init(&pgame->lock, NULL);

	pgame->level = 1;
	pgame->nsec = 1E9 - 1;
	pgame->pause_ticks = 1000;

	pgame->width = BLOCKS_MAX_COLUMNS;
	pgame->height = BLOCKS_MAX_ROWS;

	LIST_INIT(&pgame->blocks_head);

	/* We need a head of the list to properly add new blocks, so manually
	 * add the head. Then use a loop to add the rest of the starting
	 * blocks.
	 */
	for (i = 0; i < NEXT_BLOCKS_LEN +2; i++) {
		struct blocks *last, *np = malloc(sizeof *np);
		if (!np) {
			log_err("Out of memory");
			exit(EXIT_FAILURE);
		}

		randomize_block(pgame, np);

		debug("Randomized new block: %d", i);

		/* Manually add the head, then continue adding the others */
		if (i == 0) {
			LIST_INSERT_HEAD(&pgame->blocks_head, np, entries);
			continue;
		}

		/* Skip to end of list */
		for (last = HOLD_BLOCK(pgame);
		     last->entries.le_next;
		     last = last->entries.le_next)
			;

		LIST_INSERT_AFTER(last, np, entries);
	}

	/* Allocate memory for colors */
	for (i = 0; i < BLOCKS_MAX_ROWS; i++) {
		pgame->colors[i] = calloc(BLOCKS_MAX_COLUMNS,
					  sizeof(*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err("Out of memory");
			exit(EXIT_FAILURE);
		}
	}

	return 1;
}

/*
 * The inverse of the init() function. Free all allocated memory.
 */
int blocks_cleanup(struct blocks_game *pgame)
{
	if (!pgame)
		return -1;

	log_info("Cleaning game data");

	/* mutex_destroy() is undefined if mutex is locked.
	 * So try to lock it then unlock it before we destroy it
	 */
	pthread_mutex_trylock(&pgame->lock);
	pthread_mutex_unlock(&pgame->lock);

	pthread_mutex_destroy(&pgame->lock);

	/* Remove each piece in the linked list */
	while (HOLD_BLOCK(pgame)) {
		struct blocks *np = HOLD_BLOCK(pgame);
		LIST_REMOVE(np, entries);
		free(np);
	}

	for (int i = 0; i < BLOCKS_MAX_ROWS; i++)
		free(pgame->colors[i]);

	return 1;
}

/*
 * These two functions are separate threads. Game operations in here are
 * unsafe. We use pthread(7) mutexes to prevent memory corruption.
 */

/*
 * Controls the game gravity, and (attempts to)remove lines when a block
 * reaches the bottom. Indirectly creates new blocks, and updates points,
 * level, etc.
 *
 * Game is over when this function returns.
 */
void *blocks_loop(void *vp)
{
	struct blocks_game *pgame = vp;
	struct timespec ts;
	int hit;

	ts.tv_sec = 0;
	ts.tv_nsec = 0;

	if (!pgame)
		return NULL;

	/* When we read in from the database, it sets the current level
	 * for the game. Update the tick delay so we resume at proper
	 * difficulty.
	 */
	update_tick_speed(pgame);

	while (1) {
		ts.tv_nsec = pgame->nsec;
		nanosleep(&ts, NULL);

		if (pgame->lose || pgame->quit)
			break;

		if (pgame->pause && pgame->pause_ticks) {
			pgame->pause_ticks--;
			continue;
		}

		pthread_mutex_lock(&pgame->lock);

		/* Unpause the game if we're out of pause ticks */
		pgame->pause = (pgame->pause && pgame->pause_ticks);

		unwrite_cur_block(pgame);
		hit = drop_block(pgame, CURRENT_BLOCK(pgame));
		write_cur_block(pgame);

		if (hit == 0) {
			destroy_lines(pgame);
			update_cur_block(pgame);
		} else if (hit < 0) {
			exit(EXIT_FAILURE);
		}

		screen_draw_game(pgame);

		pthread_mutex_unlock(&pgame->lock);
	}

	/* remove the current piece from the board, when we write to the
	 * database it would otherwise save the location of a block in mid-air.
	 * We can't restore from blocks like that, so just remove it.
	 */
	unwrite_cur_block(pgame);

	return NULL;
}

/*
 * User input. Hopefully self explanatory.
 *
 * Input keys are currently:
 * 	F1 pause
 * 	F3 quit
 *
 * 	wasdqe
 * 	- w hard drops a piece to the bottom.
 * 	- a/d move left/right respectively.
 * 	- s soft drops one row.
 * 	- qe rotate counter clockwise/clockwise repectively.
 *
 * 	- space is used to hold the currently falling block.
 */
void *blocks_input(void *vp)
{
	struct blocks_game *pgame = vp;
	int ch;

	if (!pgame || !CURRENT_BLOCK(pgame))
		return NULL;

	while ((ch = getch())) {
		/* prevent modification of the game from blocks_loop in the
		 * other thread */
		pthread_mutex_lock(&pgame->lock);

		switch (ch) {
		case KEY_F(1):
			pgame->pause = !pgame->pause;
			goto draw;
		case KEY_F(3):
			pgame->pause = false;
			pgame->quit = true;
			goto draw;
		}

		/* remove the current piece from the board */
		unwrite_cur_block(pgame);

		/* modify it */
		switch (toupper(ch)) {
		case 'A':
			translate_block(pgame, CURRENT_BLOCK(pgame), MOVE_LEFT);
			break;
		case 'D':
			translate_block(pgame, CURRENT_BLOCK(pgame), MOVE_RIGHT);
			break;
		case 'S':
			if (drop_block(pgame, CURRENT_BLOCK(pgame)))
				CURRENT_BLOCK(pgame)->soft_drop++;
			else
				CURRENT_BLOCK(pgame)->lock_delay = 1E9 -1;
			break;
		case 'W':
			/* drop the block to the bottom of the game */
			while (drop_block(pgame, CURRENT_BLOCK(pgame)))
				CURRENT_BLOCK(pgame)->hard_drop++;
			CURRENT_BLOCK(pgame)->lock_delay = 1E9 -1;
			break;
		case 'Q':
			if (!rotate_block(pgame,
				CURRENT_BLOCK(pgame), ROT_LEFT))
			{
				try_wall_kick(pgame, CURRENT_BLOCK(pgame),
						ROT_LEFT);
			}
			break;
		case 'E':
			if (!rotate_block(pgame,
				CURRENT_BLOCK(pgame), ROT_RIGHT))
			{
				try_wall_kick(pgame, CURRENT_BLOCK(pgame),
						ROT_RIGHT);
			}
			break;
		case ' ': {
			struct blocks *tmp;

			/* We can hold each block exactly once */
			if (CURRENT_BLOCK(pgame)->hold == true)
				break;

			tmp = CURRENT_BLOCK(pgame);

			/* Effectively swap the first and second elements in
			 * the linked list. The "Current Block" is element 2.
			 * And the "Hold Block" is element 1. So we remove the
			 * current block and reinstall it at the head, pushing
			 * the hold block to the current position.
			 */
			LIST_REMOVE(tmp, entries);
			LIST_INSERT_HEAD(&pgame->blocks_head, tmp, entries);

			HOLD_BLOCK(pgame)->hold = true;
			HOLD_BLOCK(pgame)->col_off = pgame->width /2;
			HOLD_BLOCK(pgame)->row_off = 1;

			break;
			}
		}

		/* then rewrite it */
		write_cur_block(pgame);

 draw:
		screen_draw_game(pgame);
		pthread_mutex_unlock(&pgame->lock);
	}

	return NULL;
}
