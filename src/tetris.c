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

#include "tetris.h"
#include "logs.h"

#define tetris_at_yx(G, Y, X) (G->spaces[(Y)] & (1 << (X)))
#define tetris_set_yx(G, Y, X) (G->spaces[(Y)] |= (1 << (X)))
#define tetris_unset_yx(G, Y, X) (G->spaces[(Y)] &= ~(1 << (X)))

struct block {
	uint8_t soft_drop;
	uint8_t hard_drop;

	bool hold;

	uint8_t type;

	uint8_t col_off, row_off;
	struct pieces {
		int8_t x, y;
	} p[4];

	LIST_ENTRY(block) entries;
};

struct tetris {
	char id[16];
	uint16_t level;
	uint16_t lines_destroyed;
	uint16_t spaces[TETRIS_MAX_ROWS];
	uint32_t score;

	uint8_t *colors[TETRIS_MAX_ROWS];
	uint32_t tick_nsec;

	blocks_win_condition check_win;

	uint8_t bag[TETRIS_NUM_BLOCKS];

	LIST_HEAD(blocks_head, block) blocks_head;
	block *ghost;

	/* Attributes */
	bool allow_ghosts;
	bool allow_hold;
	bool allow_pause;
	bool allow_wallkicks;
	bool allow_tspin;

	/* State */
	bool paused;
	bool lose;
	bool quit;
	bool win;
	bool t_spin;
};

/** Random Generator **/
#define DIRTY_BIT 0x80

/* This is the "Random Generator" algorithm.
 * Create a 'bag' of all seven pieces, then one by one remove an element from
 * the bag. Refill the bag when it's empty.
 *
 * This helps to reduce the length of sequential pieces.
 */
void bag_random_generator(tetris *pgame) {
	uint8_t index;

	/* The order here DOES matter, edit with caution */
	uint8_t avail_blocks[] = {
		TETRIS_I_BLOCK,
		TETRIS_T_BLOCK,
		TETRIS_L_BLOCK,
		TETRIS_J_BLOCK,
		TETRIS_S_BLOCK,
		TETRIS_Z_BLOCK,
		TETRIS_O_BLOCK,
	};

	memset(pgame->bag, DIRTY_BIT, LEN(pgame->bag));

	/*
	 * From the Tetris Guidlines:
	 * 	First piece is never the O, S, or Z blocks.
	 */
	index = rand() % (LEN(pgame->bag) - 3);
	pgame->bag[0] = avail_blocks[index];
	avail_blocks[index] = DIRTY_BIT;

	/*
	 * Fill remaining bag locations with available pieces
	 */
	for (uint8_t i = 1; i < LEN(pgame->bag); i++) {

		index = rand() % (LEN(pgame->bag) - i) +1;

		size_t get_elm = 0;

		for (; get_elm < LEN(pgame->bag) && index; get_elm++)
			if (avail_blocks[get_elm] != DIRTY_BIT)
				index--;

		pgame->bag[i] = avail_blocks[get_elm-1];
		avail_blocks[get_elm-1] = DIRTY_BIT;
	}
}

int bag_next_piece(tetris *pgame) {
	static size_t index = 0;

	int ret = pgame->bag[index];

	/* Mark bag location dirty */
	pgame->bag[index++] |= DIRTY_BIT;

	if (index >= LEN(pgame->bag))
		index = 0;

	return ret;
}

int bag_is_empty(tetris *pgame) {
	for (size_t i = 0; i < LEN(pgame->bag); i++)
		if (pgame->bag[i] < DIRTY_BIT)
			return 0;

	return 1;
}
#undef DIRTY_BIT
/** Random Generator **/

/* Private helper functions */

/* Resets the block to its default positional state */
static void reset_block(block *block)
{
	int index = 0; /* used in macro def. PIECE_XY() */

	block->col_off = TETRIS_MAX_COLUMNS / 2;
	block->row_off = 1;

	block->lock_delay = 0;
	block->soft_drop = 0;
	block->hard_drop = 0;
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
#undef PIECE_XY
}

/* Randomizes block and sets the initial positions of the pieces */
static void randomize_block(tetris *pgame, block *block)
{
	/* Create a new bag if necessary, then pull the next piece from it */
	if (bag_is_empty(pgame))
		bag_random_generator(pgame);

	block->type = bag_next_piece(pgame);

	reset_block(block);
}


/* Public interface to game structure */

/* translate pieces in block horizontally. */
static int block_translate(tetris *pgame, block *block, int cmd)
{
	int dir = (cmd == TETRIS_MOVE_LEFT) ? -1 : 1; // 1 is right, -1 is left

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
int block_rotate(tetris *pgame, block *block, int cmd)
{
	/* Don't rotate O block */
	if (block->type == TETRIS_O_BLOCK)
		return 1;

	int dir = (cmd == TETRIS_ROT_LEFT) ? -1 : 1; // 1 is right, -1 is left

	int new_x[4], new_y[4];

	/* Check each piece for a collision before we write any changes */
	for (size_t i = 0; i < LEN(block->p); i++) {
		new_x[i] = block->p[i].y * (-dir);
		new_y[i] = block->p[i].x * (dir);

		int bounds_x, bounds_y;
		bounds_x = new_x[i] + block->col_off;
		bounds_y = new_y[i] + block->row_off;

		/* Check for out of bounds on each piece */
		if (bounds_x < 0 || bounds_x >= BLOCKS_MAX_COLUMNS ||
		    bounds_y < 0 || bounds_y >= BLOCKS_MAX_ROWS ||
		    /* Also check if a piece already exists here */
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	/* No collisions, so update the block position. */
	for (size_t i = 0; i < LEN(block->p); i++) {
		block->p[i].x = new_x[i];
		block->p[i].y = new_y[i];
	}

	return 1;
}

/*
 * Tetris Guidlines:
 * If rotation fails, try to move the block left, then try to rotate again.
 * If translation or rotation fails again, try to translate right, then try to
 * 	rotate again.
 */
static int block_wall_kick(tetris *pgame, block *block, int cmd)
{
	/* Try to rotate block the normal way. */
	if (block_rotate(pgame, block, cmd) == 1)
		return 1;

	/* Try to move left and rotate again. */
	if (blocks_translate(pgame, block, TETRIS_MOVE_LEFT) == 1) {
		if (block_rotate(pgame, block, cmd) == 1)
			return 1;
	}

	/* undo previous translation */
	blocks_translate(pgame, block, TETRIS_MOVE_RIGHT);

	/* Try to move right and rotate again. */
	if (blocks_translate(pgame, block, TETRIS_MOVE_RIGHT) == 1) {
		if (block_rotate(pgame, block, cmd) == 1)
			return 1;
	}

	return 0;
}

/*
 * Attempts to translate the block one row down.
 * This function is used during normal gravitational events, and during
 * user-input 'soft drop' events.
 */
static int block_fall(tetris *pgame, block *block)
{
	for (size_t i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
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
static void update_tick_speed(tetris *pgame)
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
static void update_cur_block(tetris *pgame)
{
	block *last, *np = CURRENT_BLOCK(pgame);

	LIST_REMOVE(np, entries);

	randomize_block(pgame, np);

	/* Find last block in list */
	for (last = FIRST_NEXT_BLOCK(pgame);
	     last && last->entries.le_next;
	     last = last->entries.le_next)
		;

	LIST_INSERT_AFTER(last, np, entries);
}

static void update_ghost_block(tetris *pgame, block *block)
{
	block->row_off = CURRENT_BLOCK(pgame)->row_off;
	block->col_off = CURRENT_BLOCK(pgame)->col_off;
	block->type = CURRENT_BLOCK(pgame)->type;
	memcpy(block->p, CURRENT_BLOCK(pgame)->p, sizeof block->p);

	while (blocks_fall(pgame, block))
		;
}

/* Write the current block to the game board.  */
static void write_block(tetris *pgame, block *block)
{
	int new_x[4], new_y[4];

	for (size_t i = 0; i < LEN(block->p); i++) {
		new_x[i] = block->col_off + block->p[i].x;
		new_y[i] = block->row_off + block->p[i].y;

		if (new_x[i] < 0 || new_x[i] >= BLOCKS_MAX_COLUMNS ||
		    new_y[i] < 0 || new_y[i] >= BLOCKS_MAX_ROWS)
			return;
	}

	/* Set the bit where the block exists */
	for (size_t i = 0; i < LEN(block->p); i++) {
		tetris_set_yx(pgame, new_y[i], new_x[i]);
		pgame->colors[new_y[i]][new_x[i]] = block->type;
	}
}


/*
 * We first check each row for a full line, and shift all rows above this down.
 * We currently implement naive gravity.
 */
int destroy_lines(tetris *pgame)
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
void update_points(tetris *pgame, uint8_t destroyed)
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

int tetris_set_win_condition(tetris *pgame, blocks_win_condition wc)
{
	pgame->check_win = wc;
	return 1;
}

/* Default game mode, we never win. Game continues until we lose. */
int tetris_classic()
{ return 0; }

int tetris_40_lines(tetris *pgame)
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

int tetris_timed(tetris *)
{
	(void) pgame;
	return 0;
}

/*
 * Setup the game structure for use.  Here we create the initial game pieces
 * for the game (5 'next' pieces, plus the current piece and the 'hold'
 * piece(total 7 game pieces).  We also allocate memory for the board colors,
 * and set some initial variables.
 */
int tetris_init(tetris **pmem)
{
	log_info("Initializing game data");

	*pmem = calloc(1, sizeof **pmem);
	if (*pmem == NULL) {
		log_err("Out of memory");
		return -1;
	}

	tetris *pgame = *pmem;

	bag_random_generator();
	blocks_set_win_condition(pgame, blocks_never_win);

	pgame->level = 1;
	blocks_update_tick_speed(pgame);

	pgame->allow_ghosts = true;
	pgame->ghost = malloc(sizeof *pgame->ghost);
	if (!pgame->ghost) {
		log_err("No memory for ghost block");
	}

	LIST_INIT(&pgame->blocks_head);

	/* Create and add each block to the linked list */
	for (size_t i = 0; i < NEXT_BLOCKS_LEN +2; i++) {
		block *np = malloc(sizeof *np);
		if (!np) {
			log_err("Out of memory");
			return -1;
		}

		randomize_block(pgame, np);

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

	debug("Game Initialization complete");

	return 1;
}

/*
 * The inverse of the init() function. Free all allocated memory.
 */
int tetris_cleanup(tetris *pgame)
{
	block *np;

	/* Remove each piece in the linked list */
	while ((np = pgame->blocks_head.lh_first)) {
		LIST_REMOVE(np, entries);
		free(np);
	}

	for (int i = 0; i < BLOCKS_MAX_ROWS; i++)
		free(pgame->colors[i]);

	free(pgame->ghost);
	free(pgame);

	debug("Game Cleanup complete");

	return 1;
}

/*
 * Controls the game gravity, and (attempts to)remove lines when a block
 * reaches the bottom. Indirectly creates new blocks, and updates points,
 * level, etc.
 */
static void tetris_tick(tetris *pgame)
{
	if (pgame->paused)
		return;

	if (!block_fall(pgame, CURRENT_BLOCK(pgame))) {
		write_block(pgame, CURRENT_BLOCK(pgame));

		update_points(pgame, blocks_destroy_lines(pgame));
		update_tick_speed(pgame);

		update_cur_block(pgame);
	}
}

/*
 * Blocks command processor.
 */
int tetris_cmd(tetris *pgame, int cmd)
{
	block *cur;

	if (pgame->quit || pgame->lose || pgame->win)
		return 0;

	cur = CURRENT_BLOCK(pgame);

	switch (cmd) {
	/* Game commands */
	case TETRIS_MOVE_LEFT:
	case TETRIS_MOVE_RIGHT:
		blocks_translate(pgame, cur, cmd);
		break;
	case TETRIS_ROT_LEFT:
	case TETRIS_ROT_RIGHT:
		blocks_wall_kick(pgame, cur, cmd);
		break;
	case TETRIS_MOVE_DOWN:
		if (blocks_fall(pgame, cur))
			cur->soft_drop++;
		break;
	case TETRIS_MOVE_DROP:
		/* drop the block to the bottom of the game */
		while (blocks_fall(pgame, cur))
			cur->hard_drop++;
		break;
	case TETRIS_HOLD_BLOCK:
		/* We can hold each block exactly once */
		if (cur->hold == true)
			break;

		cur->hold = true;
		reset_block(cur);

		/* Remove the current block and reinstall it at the beginning
		 * of the list. This swaps the hold and current block.
		 */
		LIST_REMOVE(cur, entries);
		LIST_INSERT_HEAD(&pgame->blocks_head, cur, entries);
		break;
	case TETRIS_SAVE_GAME:
		pgame->quit = true;
		break;

	case TETRIS_GAME_TICK:
		tetris_tick(pgame);
		break;

	/* Meta commands */
	case TETRIS_PAUSE_GAME:
		pgame->paused = !pgame->paused;
		break;

	/* Modify attributes */
	case TOGGLE_GHOSTS:
		pgame->allow_ghosts = !pgame->allow_ghosts;
		break;
	default:
		return -1;
	}

	pgame->check_win(pgame);
	update_ghost_block(pgame, pgame->ghost);

	return 1;
}
