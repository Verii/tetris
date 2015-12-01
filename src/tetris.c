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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/queue.h>
#include <sqlite3.h>

#include "conf.h"
#include "screen.h"
#include "helpers.h"
#include "tetris.h"
#include "logs.h"

/****************************/
/*  Begin Random Generator  */
/****************************/

#define DIRTY_BIT 0x80

/* This is the "Random Generator" algorithm.
 * Create a 'bag' of all seven pieces, then one by one remove an element from
 * the bag. Refill the bag when it's empty.
 *
 * This helps to reduce the length of sequential pieces.
 */
static void bag_random_generator(tetris *pgame) {
	uint8_t index;

	/* The order here DOES matter, edit with caution */
	uint8_t avail_blocks[TETRIS_NUM_BLOCKS] = {
		TETRIS_I_BLOCK,
		TETRIS_T_BLOCK,
		TETRIS_L_BLOCK,
		TETRIS_J_BLOCK,
		/* Three pieces not initially selected */
		TETRIS_S_BLOCK,
		TETRIS_Z_BLOCK,
		TETRIS_O_BLOCK,
	};

	memset(pgame->bag, DIRTY_BIT, LEN(pgame->bag));

	/*
	 * From the Tetris Guidlines:
	 * 	First piece is never the O, S, or Z blocks.
	 */
	index = random() % (LEN(pgame->bag) - 3); // [0, 3]
	pgame->bag[0] = avail_blocks[index];
	avail_blocks[index] = DIRTY_BIT;

	/*
	 * Fill remaining bag locations with available pieces.
	 *
	 * First pick a number between 1 and the number of empty spaces left in
	 * the bag (n). Then we find the (nth) unchosen piece.
	 *
	 * Add the (nth) piece to the bag at the first available position,
	 * then mark the (nth) piece dirty so we don't get it again.
	 *
	 * Repeat.
	 */
	for (uint8_t i = 1; i < LEN(pgame->bag); i++) {

		index = random() % (LEN(pgame->bag) - i) +1;

		size_t get_elm = 0;

		for (; get_elm < LEN(pgame->bag) && index; get_elm++)
			if (avail_blocks[get_elm] != DIRTY_BIT)
				index--;

		pgame->bag[i] = avail_blocks[get_elm-1];
		avail_blocks[get_elm-1] = DIRTY_BIT;
	}
}

static int bag_next_piece(tetris *pgame) {
	static size_t index = 0;

	int ret = pgame->bag[index];

	/* Mark bag location dirty */
	pgame->bag[index++] |= DIRTY_BIT;

	if (index >= LEN(pgame->bag))
		index = 0;

	return ret;
}

static int bag_is_empty(tetris *pgame) {
	for (size_t i = 0; i < LEN(pgame->bag); i++)
		if (pgame->bag[i] < DIRTY_BIT)
			return 0;

	return 1;
}
#undef DIRTY_BIT

/**************************/
/*  End Random Generator  */
/**************************/


/**********************************/
/* Begin Private helper functions */
/**********************************/

/* Resets the block to its default positional state */
static void block_reset(block *pblock)
{
	int index = 0; /* used in macro def. PIECE_XY() */

	pblock->col_off = TETRIS_MAX_COLUMNS / 2;
	pblock->row_off = 1;

	pblock->soft_drop = 0;
	pblock->hard_drop = 0;
	pblock->hold = false;
	pblock->t_spin = false;
	pblock->lock_delay = false;

#define PIECE_XY(X, Y) \
	pblock->p[index].x = X; pblock->p[index++].y = Y;

	/* The piece at (0, 0) is the pivot when we rotate. */
	PIECE_XY( 0,  0);

	switch (pblock->type) {
	case TETRIS_O_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		break;
	case TETRIS_I_BLOCK:
		pblock->col_off--;	/* center */

		PIECE_XY(-1,  0);
		PIECE_XY( 1,  0);
		PIECE_XY( 2,  0);
		break;
	case TETRIS_T_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_L_BLOCK:
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_J_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_Z_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_S_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		break;
	}
#undef PIECE_XY
}

/* Randomizes block and sets the initial positions of the pieces */
static void block_randomize(tetris *pgame, block *pblock)
{
	/* Create a new bag if necessary, then pull the next piece from it */
	if (bag_is_empty(pgame))
		bag_random_generator(pgame);

	pblock->type = bag_next_piece(pgame);

	block_reset(pblock);
}

/* translate pieces in block horizontally. */
static int block_translate(tetris *pgame, block *pblock, int cmd)
{
	/* 1 is right, -1 is left */
	int dir = (cmd == TETRIS_MOVE_LEFT) ? -1 : 1;

	/* Check each piece for a collision */
	for (size_t i = 0; i < LEN(pblock->p); i++) {
		int bounds_x, bounds_y;
		bounds_x = pblock->p[i].x + pblock->col_off + dir;
		bounds_y = pblock->p[i].y + pblock->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= TETRIS_MAX_COLUMNS ||
		    bounds_y < 0 || bounds_y >= TETRIS_MAX_ROWS ||
		    tetris_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	pblock->col_off += dir;

	return 1;
}

/* rotate pieces in blocks by either 90^ or -90^ around (0, 0) pivot */
static int block_rotate(tetris *pgame, block *pblock, int cmd)
{
	/* Don't rotate O block */
	if (pblock->type == TETRIS_O_BLOCK)
		return 1;

	/* 1 is right, -1 is left */
	int dir = (cmd == TETRIS_ROT_LEFT) ? -1 : 1;

	int new_x[4], new_y[4];

	/* Check each piece for a collision before we write any changes */
	for (size_t i = 0; i < LEN(pblock->p); i++) {
		new_x[i] = pblock->p[i].y * (-dir);
		new_y[i] = pblock->p[i].x * (dir);

		int bounds_x, bounds_y;
		bounds_x = new_x[i] + pblock->col_off;
		bounds_y = new_y[i] + pblock->row_off;

		/* Check for out of bounds on each piece */
		if (bounds_x < 0 || bounds_x >= TETRIS_MAX_COLUMNS ||
		    bounds_y < 0 || bounds_y >= TETRIS_MAX_ROWS ||
		    /* Check for collision with another piece */
		    tetris_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	/* No collisions, so update the block position. */
	for (size_t i = 0; i < LEN(pblock->p); i++) {
		pblock->p[i].x = new_x[i];
		pblock->p[i].y = new_y[i];
	}

	return 1;
}

/*
 * Tetris Guidlines:
 * If rotation fails, try to move the block left, then try to rotate again.
 * If translation or rotation fails again, try to translate right, then try to
 * 	rotate again.
 */
static int block_wall_kick(tetris *pgame, block *pblock, int cmd)
{
	/* Try to rotate block the normal way. */
	if (block_rotate(pgame, pblock, cmd) == 1)
		return 1;

	/* Try to move left and rotate again. */
	if (block_translate(pgame, pblock, TETRIS_MOVE_LEFT) == 1) {
		if (block_rotate(pgame, pblock, cmd) == 1)
			return 1;
		else
			/* undo previous translation */
			block_translate(pgame, pblock, TETRIS_MOVE_RIGHT);
	}


	/* Try to move right and rotate again. */
	if (block_translate(pgame, pblock, TETRIS_MOVE_RIGHT) == 1) {
		if (block_rotate(pgame, pblock, cmd) == 1)
			return 1;
		else
			/* undo previous translation */
			block_translate(pgame, pblock, TETRIS_MOVE_LEFT);
	}

	return 0;
}

/*
 * Attempts to translate the block one row down.
 * This function is used during normal gravitational events, and during
 * user-input 'soft drop' events.
 */
static int block_fall(tetris *pgame, block *pblock, int dir)
{
	for (size_t i = 0; i < LEN(pblock->p); i++) {
		int bounds_x, bounds_y;
		bounds_y = pblock->p[i].y + pblock->row_off + dir;
		bounds_x = pblock->p[i].x + pblock->col_off;

		if (bounds_y < 0 || bounds_y >= TETRIS_MAX_ROWS ||
		    tetris_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	pblock->row_off += dir;

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
	pgame->tick_nsec = 1E9 /speed -1;
}

/* To prevent an additional free/malloc we recycle the allocated memory.
 *
 * Remove the "Current" block and reinstall it at the end of the list
 */
static void update_cur_block(tetris *pgame)
{
	block *last, *np = CURRENT_BLOCK(pgame);

	LIST_REMOVE(np, entries);

	block_randomize(pgame, np);

	/* Find last block in list */
	for (last = FIRST_NEXT_BLOCK(pgame);
	     last && last->entries.le_next;
	     last = last->entries.le_next)
		;

	LIST_INSERT_AFTER(last, np, entries);
}

static void update_ghost_block(tetris *pgame, block *pblock)
{
	/* Copy type for block color */
	pblock->type = CURRENT_BLOCK(pgame)->type;
	/* Offsets and piece positions */
	pblock->row_off = CURRENT_BLOCK(pgame)->row_off;
	pblock->col_off = CURRENT_BLOCK(pgame)->col_off;
	memcpy(pblock->p, CURRENT_BLOCK(pgame)->p, sizeof pblock->p);

	/* Move it to the bottom of the game */
	while (block_fall(pgame, pblock, 1))
		;
}

/* Write the current block to the game board.  */
static void write_block(tetris *pgame, block *pblock)
{
	int new_x[4], new_y[4];

	for (size_t i = 0; i < LEN(pblock->p); i++) {
		new_x[i] = pblock->col_off + pblock->p[i].x;
		new_y[i] = pblock->row_off + pblock->p[i].y;

		if (new_x[i] < 0 || new_x[i] >= TETRIS_MAX_COLUMNS ||
		    new_y[i] < 0 || new_y[i] >= TETRIS_MAX_ROWS)
			return;
	}

	/* Set the bit where the block exists */
	for (size_t i = 0; i < LEN(pblock->p); i++) {
		tetris_set_yx(pgame, new_y[i], new_x[i]);
		pgame->colors[new_y[i]][new_x[i]] = pblock->type;
	}
}


/*
 * We first check each row for a full line, and shift all rows above this down.
 * We currently implement naive gravity.
 */
static int destroy_lines(tetris *pgame)
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
	for (i = TETRIS_MAX_ROWS -1; i >= 2; i--) {

		/* Fill in all bits below bit TETRIS_MAX_COLUMNS. Row
		 * populations are stored in a bit field, so we can check for a
		 * full row by comparing it to this value.
		 */
		uint32_t full_row = (1 << TETRIS_MAX_COLUMNS) -1;

		if (pgame->spaces[i] != full_row)
			continue;

		/* Move lines above destroyed line down */
		for (j = i; j > 0; j--) {
			pgame->spaces[j] = pgame->spaces[j -1];
			memcpy(pgame->colors[j], pgame->colors[j -1],
				TETRIS_MAX_COLUMNS * sizeof *pgame->colors[0]);
		}

		/* Fill top row colors with zeros. */
		memset(pgame->colors[0], 0,
			TETRIS_MAX_COLUMNS * sizeof *pgame->colors[0]);

		/* Lines are moved down, so we recheck this row. */
		i++;
		destroyed++;
	}

	pgame->lines_destroyed += destroyed;
	return destroyed;
}

static void update_level(tetris *pgame)
{
	int lines = pgame->lines_destroyed; // to make things fit

	/* level^2 + level*3 + 2 */
	while (lines >= (pgame->level*pgame->level + 3*pgame->level +2)) {
		pgame->level++;
		logs_to_game("Level up! Speed up!");
	}
}

/* Get points based on number of lines destroyed, or if 0, get points based on
 * how far the block fell.
 *
 * We do a bit of logic to add points to the game. Line clears which are
 * considered difficult(as per the Tetris Guidlines) will yield more points by
 * a factor of 3/2 during consecutive moves.
 */
static void update_points(tetris *pgame, uint8_t destroyed)
{
	size_t point_mod = 0;

	if (destroyed == 0 || destroyed > 4)
		goto done;

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

	/* Add difficulty bonus for consecutive difficult moves */
	if (pgame->difficult == true)
		point_mod = (point_mod * 3) /2;

	/* point modifier, "difficult" line clears earn more over time.
	 * a tetris (4 line clears) counts for 1 difficult move.
	 *
	 * consecutive difficult moves inc. points by 3/2
	 */
	if (destroyed == 4) {
		logs_to_game("Tetris!");
		pgame->difficult = true;
	} else if (CURRENT_BLOCK(pgame)->t_spin) {
		logs_to_game("T spin!");
		pgame->difficult = true;
	} else {
		/* We lose our difficulty multipliers on easy moves */
		pgame->difficult = false;
	}

done :{
	int score_inc = (point_mod * pgame->level)
		+ CURRENT_BLOCK(pgame)->soft_drop
		+ (CURRENT_BLOCK(pgame)->hard_drop * 2);

#if defined(DEBUG)
	logs_to_game("+%d", score_inc);
#endif
	pgame->score += score_inc;
      }
}

/*
 * Controls the game gravity, and (attempts to)remove lines when a block
 * reaches the bottom. Indirectly creates new blocks, and updates points,
 * level, etc.
 */
static void tetris_tick(tetris *pgame)
{
	/* If the player dropped the block give them an extra game tick */
	if (pgame->enable_lock_delay &&
	    CURRENT_BLOCK(pgame)->hard_drop &&
	    CURRENT_BLOCK(pgame)->lock_delay == false) {
		CURRENT_BLOCK(pgame)->lock_delay = true;
		return;
	}

	/* Check for T-Spin */
	/* A T-Spin is counted if a T block cannot move left, right, or
	 * up and a line is cleared.
	 */
	if (pgame->enable_tspins &&
	    CURRENT_BLOCK(pgame)->type == TETRIS_T_BLOCK) {

		CURRENT_BLOCK(pgame)->t_spin = true;

		/* Try to move left, if it succeeds, we move it back
		 * and fail
		 */
		if (block_translate(pgame, CURRENT_BLOCK(pgame), TETRIS_MOVE_LEFT)) {
			block_translate(pgame, CURRENT_BLOCK(pgame), TETRIS_MOVE_RIGHT);
			CURRENT_BLOCK(pgame)->t_spin = false;
		}

		/* Try to move right, if successful move back and
		 * fail. */
		if (block_translate(pgame, CURRENT_BLOCK(pgame), TETRIS_MOVE_RIGHT)) {
			block_translate(pgame, CURRENT_BLOCK(pgame), TETRIS_MOVE_LEFT);
			CURRENT_BLOCK(pgame)->t_spin = false;
		}

		/* Try to move up, if successful, move down and fail */
		if (block_fall(pgame, CURRENT_BLOCK(pgame), -1)) {
			block_fall(pgame, CURRENT_BLOCK(pgame), 1);
			CURRENT_BLOCK(pgame)->t_spin = false;
		}
	}

	if (!block_fall(pgame, CURRENT_BLOCK(pgame), 1)) {

		write_block(pgame, CURRENT_BLOCK(pgame));

		int lines = destroy_lines(pgame);

		update_level(pgame);
		update_tick_speed(pgame);
		update_points(pgame, lines);
		update_cur_block(pgame);
	}
}

/* Game Modes */

/* Default game mode, we never win. Game continues until we lose. */
static int tetris_classic(tetris *pgame)
{
	pgame->win = false;
	return 0;
}

/* We win when we've destroyed 40 or more lines */
static int tetris_40_lines(tetris *pgame)
{
	if (pgame->lines_destroyed >= 40) {
		pgame->win = true;
		return 1;
	}
	return 0;
}

/* TODO, play for X minutes */
static int tetris_timed(tetris *pgame)
{
	pgame->win = false;
	return 0;
}


/************************************/
/*   End Private helper functions   */
/************************************/


/************************************/
/*  Begin Public interface to game  */
/************************************/

/*
 * Setup the game structure for use.  Here we create the initial game pieces
 * for the game (5 'next' pieces, plus the current piece and the 'hold'
 * piece(total 7 game pieces).  We also allocate memory for the board colors,
 * and set some initial variables.
 */
int tetris_init(tetris **res)
{
	tetris *pgame;
	if ((pgame = calloc(1, sizeof *pgame)) == NULL) {
		log_err("Out of memory");
		return -1;
	}

	/* So we can save it later ... ? */
	pgame->rseed = time(NULL);
	srandom(pgame->rseed);

	tetris_set_gamemode(pgame, TETRIS_CLASSIC);
	pgame->level = 1;
	update_tick_speed(pgame);

	/* Add pieces to bag */
	bag_random_generator(pgame);
	LIST_INIT(&pgame->blocks_head);

	/* Create and add each block to the linked list */
	for (size_t i = 0; i < TETRIS_NEXT_BLOCKS_LEN +2; i++) {
		block *np = malloc(sizeof *np);
		if (!np) {
			log_err("Out of memory");
			goto mem_err;
		}

		/* randomize() assigns each block a random block (T, Z, etc.)
		 * And it resets the block to its original place(the top of the
		 * game). As the game goes on, we just use this function to get
		 * new blocks. We don't free/malloc new memory for each new
		 * block.
		 */
		block_randomize(pgame, np);

		LIST_INSERT_HEAD(&pgame->blocks_head, np, entries);
	}

	pgame->ghost_block = malloc(sizeof *pgame->ghost_block);
	if (!pgame->ghost_block) {
		log_err("Out of memory");
		goto mem_err;
	}

	/* Allocate memory for colors */
	for (size_t i = 0; i < TETRIS_MAX_ROWS; i++) {
		pgame->colors[i] = calloc(TETRIS_MAX_COLUMNS,
					  sizeof(*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err("Out of memory");
			goto mem_err;
		}
	}

	*res = pgame;
	debug("Game initializing complete");

	return 1;

mem_err:
	*res = NULL;
	free(pgame);
	return -1;
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

	for (int i = 0; i < TETRIS_MAX_ROWS; i++)
		free(pgame->colors[i]);

	free(pgame->ghost_block);
	free(pgame);

	debug("Game Cleanup complete");

	return 1;
}

/*
 * Blocks command processor.
 */
int tetris_cmd(tetris *pgame, int cmd)
{
	/* "fail" if these are true so we can escape events_main_loop() */
	if (pgame->quit || pgame->lose || pgame->win)
		return -1;

	/* While paused, we can only unpause or quit */
	if (pgame->paused &&
	   !(cmd == TETRIS_PAUSE_GAME || cmd == TETRIS_QUIT_GAME))
		return 0;

	block *cur = CURRENT_BLOCK(pgame);

	/* Block lock delays are enabled after a block is hard dropped to the
	 * bottom of the board. The game will wait one additional game tick,
	 * or for a single movement command, before locking the block into place.
	 */

	/* This would be the command *after* a block has been hard dropped. */
	bool additional_tick = cur->lock_delay || cur->hard_drop;

	switch (cmd) {
	case TETRIS_MOVE_LEFT:
	case TETRIS_MOVE_RIGHT:
		block_translate(pgame, cur, cmd);
		break;

	case TETRIS_ROT_LEFT:
	case TETRIS_ROT_RIGHT:
		if (pgame->enable_wallkicks)
			block_wall_kick(pgame, cur, cmd);
		else
			block_rotate(pgame, cur, cmd);
		break;

	case TETRIS_MOVE_DOWN:
		if (block_fall(pgame, cur, 1))
			cur->soft_drop++;
		break;

	case TETRIS_MOVE_DROP:
		/* drop the block to the bottom of the game */
		while (block_fall(pgame, cur, 1))
			cur->hard_drop++;
		break;
	}

	if (additional_tick &&
	    (cmd == TETRIS_MOVE_LEFT || cmd == TETRIS_MOVE_RIGHT ||
	    cmd == TETRIS_ROT_LEFT || cmd == TETRIS_ROT_RIGHT ||
	    cmd == TETRIS_MOVE_DOWN || cmd == TETRIS_MOVE_DROP)) {

		/* have we ticked once already? */
		if (cur->lock_delay == false)
			tetris_tick(pgame); // first one here is skipped
		/* lock block to board */
		tetris_tick(pgame);
	}

	switch (cmd) {
	case TETRIS_HOLD_BLOCK:
		/* We can hold each block exactly once */
		if (cur->hold == true) {
			logs_to_game("Block has already been held.");
			break;
		}

		block_reset(cur);
		cur->hold = true;

		/* Remove the current block and reinstall it at the beginning
		 * of the list. This swaps the hold and current block.
		 */
		LIST_REMOVE(cur, entries);
		LIST_INSERT_HEAD(&pgame->blocks_head, cur, entries);
		break;

	case TETRIS_QUIT_GAME:
		pgame->quit = true;
		break;

	case TETRIS_PAUSE_GAME:
		pgame->paused = !pgame->paused;
		pgame->difficult = false; // Lose difficulty bonus when we pause
		break;

	case TETRIS_GAME_TICK:
		tetris_tick(pgame);
		break;
	}

	if (pgame->check_win)
		pgame->check_win(pgame);

	if (pgame->enable_ghosts)
		update_ghost_block(pgame, pgame->ghost_block);

	return 1;
}

int tetris_set_name(tetris *pgame, const char *name)
{
	strncpy(pgame->id, name, sizeof pgame->id);
	if (sizeof pgame->id > 0)
		pgame->id[sizeof pgame->id -1] = '\0';
	return 1;
}

int tetris_set_dbfile(tetris *pgame, const char *dbfile)
{
	strncpy(pgame->db_file, dbfile, sizeof pgame->db_file);
	if (sizeof pgame->db_file > 0)
		pgame->db_file[sizeof pgame->db_file -1] = '\0';
	return 1;
}


int tetris_set_gamemode(tetris *pgame, enum TETRIS_GAMES gm)
{
	switch (gm) {
	case TETRIS_40_LINES:
		tetris_set_ghosts(pgame, 1);
		tetris_set_wallkicks(pgame, 1);
		tetris_set_tspins(pgame, 1);
		tetris_set_lockdelay(pgame, 1);

		strncpy(pgame->gamemode, "40 Lines", sizeof pgame->gamemode);
		pgame->check_win = tetris_40_lines;
		break;
	case TETRIS_TIMED:
		/* XXX, change my name when we define tetris_timed() */
		strncpy(pgame->gamemode, "Classic", sizeof pgame->gamemode);
		pgame->check_win = tetris_timed;
		break;
	case TETRIS_INFINITY:
		tetris_set_ghosts(pgame, 1);
		tetris_set_wallkicks(pgame, 1);
		tetris_set_tspins(pgame, 1);
		tetris_set_lockdelay(pgame, 1);

		strncpy(pgame->gamemode, "Infinity", sizeof pgame->gamemode);
		pgame->check_win = tetris_classic;
		break;
	case TETRIS_CLASSIC:
	default:
		tetris_set_ghosts(pgame, 0);
		tetris_set_wallkicks(pgame, 0);
		tetris_set_tspins(pgame, 0);
		tetris_set_lockdelay(pgame, 0);

		strncpy(pgame->gamemode, "Classic", sizeof pgame->gamemode);
		pgame->check_win = tetris_classic;
		break;
	}

	return 1;
}


enum TETRIS_GAME_STATE
tetris_get_state(tetris *pgame)
{
	if (pgame->win) return TETRIS_WIN;
	if (pgame->lose) return TETRIS_LOSE;
	if (pgame->paused) return TETRIS_PAUSED;
	if (pgame->quit) return TETRIS_QUIT;
	return -1;
}

int tetris_get_name(tetris *pgame, char *ret, size_t len)
{
	strncpy(ret, pgame->id, len);
	if (len > 0)
		ret[len -1] = '\0';
	return 1;
}

int tetris_get_dbfile(tetris *pgame, char *ret, size_t len)
{
	strncpy(ret, pgame->db_file, len);
	if (len > 0)
		ret[len -1] = '\0';
	return 1;
}

/************************************/
/*   End Public interface to game   */
/************************************/
