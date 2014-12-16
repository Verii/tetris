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

#include <ctype.h>
#include <math.h>
#include <ncurses.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bag.h"
#include "blocks.h"
#include "logs.h"

/* Private helper functions */

/* Resets the block to its default positional state */
static void reset_block(struct blocks *block)
{
	int index = 0; /* used in macro def. PIECE_XY() */

	block->col_off = BLOCKS_MAX_COLUMNS / 2;
	block->row_off = 1;

	block->lock_delay = 0;
	block->soft_drop = 0;
	block->hard_drop = 0;
	block->t_spin = false;
	block->hold = false;

#define PIECE_XY(X, Y) \
	block->p[index].x = X; block->p[index++].y = Y;

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

/* Randomizes block and sets the initial positions of the pieces */
static void randomize_block(struct blocks *block)
{
	/* Create a new bag if necessary, then pull the next piece from it */
	if (bag_is_empty())
		bag_random_generator();

	block->type = bag_next_piece();

	reset_block(block);
}


/* Public interface to game structure */

/* translate pieces in block horizontally. */
int blocks_translate(struct blocks_game *pgame, struct blocks *block, enum blocks_input_cmd cmd)
{
	int dir = (cmd == MOVE_LEFT) ? -1 : 1; // 1 is right, -1 is left

	/* Check each piece for a collision */
	for (size_t i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
		bounds_x = block->p[i].x + block->col_off + dir;
		bounds_y = block->p[i].y + block->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= BLOCKS_MAX_COLUMNS ||
		    bounds_y < 0 || bounds_y >= BLOCKS_MAX_ROWS ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	block->col_off += dir;

	return 1;
}

/* rotate pieces in blocks by either 90^ or -90^ around (0, 0) pivot */
int blocks_rotate(struct blocks_game *pgame, struct blocks *block, enum blocks_input_cmd cmd)
{
	/* Don't rotate O block */
	if (block->type == O_BLOCK)
		return 1;

	int dir = (cmd == ROT_LEFT) ? -1 : 1; // -1 is LEFT, 1 is RIGHT

	/* Check each piece for a collision before we write any changes */
	for (size_t i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
		bounds_x = block->p[i].y * (-dir) + block->col_off;
		bounds_y = block->p[i].x * (dir) + block->row_off;

		/* Check for out of bounds on each piece */
		if (bounds_x < 0 || bounds_x >= BLOCKS_MAX_COLUMNS ||
		    bounds_y < 0 || bounds_y >= BLOCKS_MAX_ROWS ||
		    /* Also check if a piece already exists here */
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	/* No collisions, so update the block position. */
	for (size_t i = 0; i < LEN(block->p); i++) {
		int new_x, new_y;
		new_x = block->p[i].y * (-dir);
		new_y = block->p[i].x * (dir);

		block->p[i].x = new_x;
		block->p[i].y = new_y;
	}

	return 1;
}

/*
 * Tetris Guidlines:
 * If rotation fails, try to move the block left, then try to rotate again.
 * If translation or rotation fails again, try to translate right, then try to
 * 	rotate again.
 */
int blocks_wall_kick(struct blocks_game *pgame, struct blocks *block, enum blocks_input_cmd cmd)
{
	/* Try to move left and rotate again. */
	if (blocks_translate(pgame, block, MOVE_LEFT) == 1) {
		if (blocks_rotate(pgame, block, cmd) == 1)
			return 1;
	}

	/* undo previous translation */
	blocks_translate(pgame, block, MOVE_RIGHT);

	/* Try to move right and rotate again. */
	if (blocks_translate(pgame, block, MOVE_RIGHT) == 1) {
		if (blocks_rotate(pgame, block, cmd) == 1)
			return 1;
	}

	return 0;
}

/*
 * Attempts to translate the block one row down.
 * This function is used during normal gravitational events, and during
 * user-input 'soft drop' events.
 */
int blocks_fall(struct blocks_game *pgame, struct blocks *block)
{
	for (size_t i = 0; i < LEN(block->p); i++) {
		size_t bounds_x, bounds_y;
		bounds_y = block->p[i].y + block->row_off + 1;
		bounds_x = block->p[i].x + block->col_off;

		if (bounds_y >= BLOCKS_MAX_ROWS ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	block->row_off++;

	return 1;
}

/*
 * Decrease the tick delay of the falling block.
 * Algorithm will most likely change. It currently follows the arctan curve.
 * Not quite sure that I like it, though.
 */
void blocks_update_tick_speed(struct blocks_game *pgame)
{
	double speed = 1.0f;

	speed += atan(pgame->level / 5.0) * 2 / PI * 3;
	pgame->nsec = 1E9 /speed -1;
}

/*
 * To prevent an additional free/malloc we recycle the allocated memory.
 *
 * First remove it from its current state, causing the next block to 'fall'
 * into place. Then we randomize it and reinstall it at the end of the list.
 */
void blocks_update_cur_block(struct blocks_game *pgame)
{
	struct blocks *last, *np = CURRENT_BLOCK(pgame);

	LIST_REMOVE(np, entries);

	randomize_block(np);

	/* Find last block in list */
	for (last = FIRST_NEXT_BLOCK(pgame);
	     last && last->entries.le_next;
	     last = last->entries.le_next)
		;

	LIST_INSERT_AFTER(last, np, entries);
}

void blocks_update_ghost_block(struct blocks_game *pgame, struct blocks *block)
{
	block->row_off = CURRENT_BLOCK(pgame)->row_off;
	block->col_off = CURRENT_BLOCK(pgame)->col_off;
	block->type = CURRENT_BLOCK(pgame)->type;
	memcpy(block->p, CURRENT_BLOCK(pgame)->p, sizeof block->p);

	while (blocks_fall(pgame, block))
		;
}

/* Write the current block to the game board.  */
void blocks_write_block(struct blocks_game *pgame, struct blocks *block)
{
	int px[4], py[4];

	for (size_t i = 0; i < LEN(block->p); i++) {
		py[i] = block->row_off + block->p[i].y;
		px[i] = block->col_off + block->p[i].x;

		if (px[i] < 0 || px[i] >= BLOCKS_MAX_COLUMNS ||
		    py[i] < 0 || py[i] >= BLOCKS_MAX_ROWS)
			return;
	}

	/* pgame->spaces is an array of bit fields, 1 per row */
	for (size_t i = 0; i < LEN(block->p); i++) {
		/* Set the bit where the block exists */
		pgame->spaces[py[i]] |= (1 << px[i]);
		pgame->colors[py[i]][px[i]] = block->type;
	}
}


/*
 * We first check each row for a full line, and shift all rows above this down.
 * We currently implement naive gravity.
 */
int blocks_destroy_lines(struct blocks_game *pgame)
{
	uint8_t i, j;

	/* Lines destroyed this turn. <= 4. */
	uint8_t destroyed = 0;

	/* The first two rows are 'above' the game, that's where the new blocks
	 * come into existence. We lose if there's ever a block there. */
	if (pgame->spaces[0] || pgame->spaces[1])
		pgame->lose = true;

	/* Check each row for a full line,
	 * we check in reverse to ease moving the rows down when we find a full
	 * row. Ignore the top two rows, which we checked above.
	 */
	for (i = BLOCKS_MAX_ROWS -1; i >= 2; i--) {

		/* Fill in all bits below bit BLOCKS_MAX_COLUMNS. Row
		 * populations are stored in a bit field, so we can check for a
		 * full row by comparing it to this value.
		 */
		uint32_t full_row = (1 << BLOCKS_MAX_COLUMNS) -1;

		if (pgame->spaces[i] != full_row)
			continue;

		/* Move lines above destroyed line down */
		for (j = i; j > 0; j--)
			pgame->spaces[j] = pgame->spaces[j -1];

		/* Lines are moved down, so we recheck this row. */
		i++;
		destroyed++;
	}

	return destroyed;
}

/* Get points based on number of lines destroyed, or if 0, get points based on
 * how far the block fell.
 *
 * We do a bit of logic to add points to the game. Line clears which are
 * considered difficult(as per the Tetris Guidlines) will yield more points by
 * a factor of 3/2 during consecutive moves.
 *
 * We also update the level here. Which currently only affects the speed of the
 * falling blocks, and counts as a multiplier for points(i.e. level 2 will
 * yield (points *2)). The level increments when we destroy (level *2) +2 lines.
 */
void blocks_update_points(struct blocks_game *pgame, uint8_t destroyed)
{
	size_t point_mod = 0;

	if (destroyed == 0 || destroyed > 4)
		goto done;

	/* Check for level up, Increase speed */
	pgame->lines_destroyed += destroyed;
	if (pgame->lines_destroyed >= (pgame->level * 2 + 2)) {
		pgame->lines_destroyed -= (pgame->level * 2 + 2);
		pgame->level++;
		blocks_update_tick_speed(pgame);

		logs_to_game("Level up! Speed up!");
	}

	/* Number of lines destroyed in move
	 * Point values from the Tetris Guidlines
	 */
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
			point_mod = 800;
			break;
	}

	/* point modifier, "difficult" line clears earn more over time.
	 * a tetris (4 line clears) counts for 1 difficult move.
	 *
	 * difficult values >1 boost points by 3/2
	 */
	static size_t difficult = 0;

	if (destroyed == 4) {
		logs_to_game("Tetris!");
		difficult++;
	} else if (CURRENT_BLOCK(pgame)->t_spin) {
		logs_to_game("T spin!");
		difficult++;
	} else {
		/* We lose our difficulty multipliers on easy moves */
		difficult = 0;
	}

	/* Add difficulty bonus for consecutive difficult moves */
	if (difficult > 1)
		point_mod = (point_mod * 3) /2;

done:
	pgame->score += (point_mod * pgame->level)
		+ CURRENT_BLOCK(pgame)->soft_drop
		+ (CURRENT_BLOCK(pgame)->hard_drop * 2);
}

int blocks_set_win_condition(struct blocks_game *pgame, blocks_win_condition wc)
{
	pgame->check_win = wc;
	return 1;
}

/* Default game mode, we never win. Game continues until we die. */
int blocks_never_win(struct blocks_game *pgame)
{
	(void) pgame;
	return 0;
}

int blocks_40_lines(struct blocks_game *pgame)
{
	/* This doesn't currently work, because lines_destroyed is how many
	 * lines we've removed since our last level up.
	 * TODO
	 */
	if (pgame->lines_destroyed >= 40) {
		pgame->win = true;
		return 1;
	}

	return 0;
}

/*
 * Setup the game structure for use.  Here we create the initial game pieces
 * for the game (5 'next' pieces, plus the current piece and the 'hold'
 * piece(total 7 game pieces).  We also allocate memory for the board colors,
 * and set some initial variables.
 */
int blocks_init(struct blocks_game **pmem)
{
	log_info("Initializing game data");
	*pmem = calloc(1, sizeof **pmem);
	if (*pmem == NULL) {
		log_err("Out of memory");
		return -1;
	}

	struct blocks_game *pgame = *pmem;

	bag_random_generator();
	blocks_set_win_condition(pgame, blocks_never_win);

	pgame->level = 1;
	blocks_update_tick_speed(pgame);

	pgame->ghosts = true;
	pgame->ghost = malloc(sizeof *pgame->ghost);
	if (!pgame->ghost) {
		log_err("No memory for ghost block");
	}

	LIST_INIT(&pgame->blocks_head);

	/* Create and add each block to the linked list */
	for (size_t i = 0; i < NEXT_BLOCKS_LEN +2; i++) {
		struct blocks *np = malloc(sizeof *np);
		if (!np) {
			log_err("Out of memory");
			return -1;
		}

		randomize_block(np);

		LIST_INSERT_HEAD(&pgame->blocks_head, np, entries);
	}

	/* Allocate memory for colors */
	for (size_t i = 0; i < BLOCKS_MAX_ROWS; i++) {
		pgame->colors[i] = calloc(BLOCKS_MAX_COLUMNS,
					  sizeof(*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err("Out of memory");
			return -1;
		}
	}

	log_info("Game successfully initialized");

	return 1;
}

/*
 * The inverse of the init() function. Free all allocated memory.
 */
int blocks_cleanup(struct blocks_game *pgame)
{
	log_info("Cleaning game data");

	struct blocks *np;

	/* Remove each piece in the linked list */
	while ((np = pgame->blocks_head.lh_first)) {
		LIST_REMOVE(np, entries);
		free(np);
	}

	for (int i = 0; i < BLOCKS_MAX_ROWS; i++)
		free(pgame->colors[i]);

	free(pgame);

	return 1;
}

/*
 * Controls the game gravity, and (attempts to)remove lines when a block
 * reaches the bottom. Indirectly creates new blocks, and updates points,
 * level, etc.
 */
void blocks_tick(struct blocks_game *pgame)
{
	if (pgame->pause || pgame->lose || pgame->quit || pgame->win)
		return;

	if (!blocks_fall(pgame, CURRENT_BLOCK(pgame))) {
		blocks_write_block(pgame, CURRENT_BLOCK(pgame));

		/* We lose if there's any blocks in the first two rows */
		if (pgame->spaces[0] || pgame->spaces[1])
			pgame->lose = true;

		blocks_update_points(pgame, blocks_destroy_lines(pgame));
		blocks_update_tick_speed(pgame);

		blocks_update_cur_block(pgame);
		blocks_update_ghost_block(pgame, pgame->ghost);
	}

	if (pgame->check_win)
		pgame->check_win(pgame);
}

/*
 * User input. Hopefully self explanatory.
 *
 * Input keys are currently:
 * 	- P pause
 * 	- O save/quit
 * 	- G toggle ghost
 * 	- W hard drops a piece to the bottom.
 * 	- [AD] move left/right respectively.
 * 	- S soft drops one row.
 * 	- [QE] rotate counter clockwise/clockwise repectively.
 * 	- (space) is used to hold the currently falling block.
 */
int blocks_input(struct blocks_game *pgame, int ch)
{
	struct blocks *tmp;

	if (pgame->quit || pgame->lose || pgame->win)
		return -1;

	switch (toupper(ch)) {
	case 'G':
		pgame->ghosts = !pgame->ghosts;
		break;
	case 'P':
		pgame->pause = !pgame->pause;
		break;
	case 'O':
		pgame->quit = true;
		return -1;
	}

	switch (toupper(ch)) {
	case 'A':
		blocks_translate(pgame, CURRENT_BLOCK(pgame), MOVE_LEFT);
		break;
	case 'D':
		blocks_translate(pgame, CURRENT_BLOCK(pgame), MOVE_RIGHT);
		break;
	case 'S':
		if (blocks_fall(pgame, CURRENT_BLOCK(pgame)))
			CURRENT_BLOCK(pgame)->soft_drop++;
		break;
	case 'W':
		/* drop the block to the bottom of the game */
		while (blocks_fall(pgame, CURRENT_BLOCK(pgame)))
			CURRENT_BLOCK(pgame)->hard_drop++;

		CURRENT_BLOCK(pgame)->lock_delay = 1E9 -1;
		break;
	case 'Q':
		if (!blocks_rotate(pgame, CURRENT_BLOCK(pgame), ROT_LEFT))
		{
			blocks_wall_kick(pgame, CURRENT_BLOCK(pgame), ROT_LEFT);
		}
		break;
	case 'E':
		if (!blocks_rotate(pgame, CURRENT_BLOCK(pgame), ROT_RIGHT))
		{
			blocks_wall_kick(pgame, CURRENT_BLOCK(pgame), ROT_RIGHT);
		}
		break;
	case ' ':
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

		reset_block(HOLD_BLOCK(pgame));
		HOLD_BLOCK(pgame)->hold = true;
		break;
	}

	blocks_update_ghost_block(pgame, pgame->ghost);

	return 1;
}
