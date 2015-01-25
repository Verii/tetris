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

/* Check if the given (Y, X) contains a block on the game board (G). */
#define tetris_at_yx(G, Y, X)		(G->spaces[(Y)] & (1 << (X)))

/* Set/Unset the given coordinate as having a piece on game board G. */
#define tetris_set_yx(G, Y, X)		(G->spaces[(Y)] |= (1 << (X)))
#define tetris_unset_yx(G, Y, X)	(G->spaces[(Y)] &= ~(1 << (X)))

/* First, Second, and Third elements in the linked list */
#define HOLD_BLOCK(G) ((G)->blocks_head.lh_first)
#define CURRENT_BLOCK(G) (HOLD_BLOCK((G))->entries.le_next)
#define FIRST_NEXT_BLOCK(G) (CURRENT_BLOCK((G))->entries.le_next)

struct tetris {
	uint16_t spaces[TETRIS_MAX_ROWS];
	uint16_t level;
	uint32_t lines_destroyed;
	uint32_t score;

	uint8_t *colors[TETRIS_MAX_ROWS];
	uint32_t tick_nsec;

	int (*check_win)(tetris *);

	uint8_t bag[TETRIS_NUM_BLOCKS];

	LIST_HEAD(blocks_head, block) blocks_head;
	block *ghost;

	/* Attributes */
	char id[16];
	bool wallkicks;
	bool tspins;
	bool ghosts;

	/* State */
	bool paused;
	bool win;
	bool lose;
	bool quit;
	bool difficult; // successive difficult moves

	sqlite3 *db_handle;
	char db_file[256];

	struct tetris_score_head score_head;
};

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
	index = rand() % (LEN(pgame->bag) - 3); // [0, 3]
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

		index = rand() % (LEN(pgame->bag) - i) +1;

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

#define PIECE_XY(X, Y) \
	pblock->p[index].x = X; pblock->p[index++].y = Y;

	/* The piece at (0, 0) is the pivot when we rotate */
	switch (pblock->type) {
	case TETRIS_O_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		break;
	case TETRIS_I_BLOCK:
		pblock->col_off--;	/* center */

		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		PIECE_XY( 2,  0);
		break;
	case TETRIS_T_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_L_BLOCK:
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_J_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_Z_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY( 0,  0);
		PIECE_XY( 1,  0);
		break;
	case TETRIS_S_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
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
int block_rotate(tetris *pgame, block *pblock, int cmd)
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
	}

	/* undo previous translation */
	block_translate(pgame, pblock, TETRIS_MOVE_RIGHT);

	/* Try to move right and rotate again. */
	if (block_translate(pgame, pblock, TETRIS_MOVE_RIGHT) == 1) {
		if (block_rotate(pgame, pblock, cmd) == 1)
			return 1;
	}

	return 0;
}

/*
 * Attempts to translate the block one row down.
 * This function is used during normal gravitational events, and during
 * user-input 'soft drop' events.
 */
static int block_fall(tetris *pgame, block *pblock)
{
	for (size_t i = 0; i < LEN(pblock->p); i++) {
		int bounds_x, bounds_y;
		bounds_y = pblock->p[i].y + pblock->row_off + 1;
		bounds_x = pblock->p[i].x + pblock->col_off;

		if (bounds_y >= TETRIS_MAX_ROWS ||
		    tetris_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	pblock->row_off++;

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
	pblock->row_off = CURRENT_BLOCK(pgame)->row_off;
	pblock->col_off = CURRENT_BLOCK(pgame)->col_off;
	pblock->type = CURRENT_BLOCK(pgame)->type;
	memcpy(pblock->p, CURRENT_BLOCK(pgame)->p, sizeof pblock->p);

	while (block_fall(pgame, pblock))
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

	logs_to_game("+%d", score_inc);
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
	if (!block_fall(pgame, CURRENT_BLOCK(pgame))) {
		write_block(pgame, CURRENT_BLOCK(pgame));


		int lines = destroy_lines(pgame);

		update_level(pgame);
		update_tick_speed(pgame);
		update_points(pgame, lines);
		update_cur_block(pgame);
	}
}

static int tetris_db_open(tetris *pgame)
{
	int status = sqlite3_open(pgame->db_file, &pgame->db_handle);
	if (status != SQLITE_OK) {
		log_warn("DB cannot be opened. (%d)", status);
		return -1;
	}

	return 1;
}

static void tetris_db_close(tetris *pgame)
{
	if (!pgame->db_handle) {
		log_warn("Trying to close un-initialized database.");
		return;
	}

	sqlite3_close(pgame->db_handle);
}


/************************************/
/*  Begin Public interface to game  */
/************************************/

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

	bag_random_generator(pgame);
	tetris_set_win_condition(pgame, tetris_classic);

	pgame->level = 1;
	update_tick_speed(pgame);

	pgame->ghost = malloc(sizeof *pgame->ghost);
	if (!pgame->ghost) {
		log_err("Out of memory");
		return -1;
	}

	LIST_INIT(&pgame->blocks_head);
	TAILQ_INIT(&pgame->score_head);

	/* Create and add each block to the linked list */
	for (size_t i = 0; i < TETRIS_NEXT_BLOCKS_LEN +2; i++) {
		block *np = malloc(sizeof *np);
		if (!np) {
			log_err("Out of memory");
			return -1;
		}

		block_randomize(pgame, np);

		LIST_INSERT_HEAD(&pgame->blocks_head, np, entries);
	}

	/* Allocate memory for colors */
	for (size_t i = 0; i < TETRIS_MAX_ROWS; i++) {
		pgame->colors[i] = calloc(TETRIS_MAX_COLUMNS,
					  sizeof(*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err("Out of memory");
			return -1;
		}
	}

	tetris_resume_state(pgame);

	debug("Game Initialization complete");

	return 1;
}

/*
 * The inverse of the init() function. Free all allocated memory.
 */
int tetris_cleanup(tetris *pgame)
{
	block *np;

	if (pgame->lose || pgame->win) {
		tetris_save_score(pgame);
	} else {
		tetris_save_state(pgame);
	}

	/* Remove each piece in the linked list */
	while ((np = pgame->blocks_head.lh_first)) {
		LIST_REMOVE(np, entries);
		free(np);
	}

	for (int i = 0; i < TETRIS_MAX_ROWS; i++)
		free(pgame->colors[i]);

	free(pgame->ghost);
	free(pgame);

	debug("Game Cleanup complete");

	return 1;
}

/*
 * Blocks command processor.
 */
int tetris_cmd(tetris *pgame, int cmd)
{
	if (pgame->quit || pgame->lose || pgame->win)
		return -1;

	if (pgame->paused &&
	   !(cmd == TETRIS_PAUSE_GAME || cmd == TETRIS_QUIT_GAME))
		return 0;

	block *cur = CURRENT_BLOCK(pgame);

	switch (cmd) {
	case TETRIS_MOVE_LEFT:
	case TETRIS_MOVE_RIGHT:
		block_translate(pgame, cur, cmd);
		break;

	case TETRIS_ROT_LEFT:
	case TETRIS_ROT_RIGHT:
		block_wall_kick(pgame, cur, cmd);
		break;

	case TETRIS_MOVE_DOWN:
		if (block_fall(pgame, cur))
			cur->soft_drop++;
		break;

	case TETRIS_MOVE_DROP:
		/* drop the block to the bottom of the game */
		while (block_fall(pgame, cur))
			cur->hard_drop++;
		break;

	case TETRIS_HOLD_BLOCK:
		/* We can hold each block exactly once */
		if (cur->hold == true)
			break;

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

	default:
		return 0;
	}

	pgame->check_win(pgame);
	update_ghost_block(pgame, pgame->ghost);

	return 1;
}

int tetris_set_attr(tetris *pgame, int attrib, ...)
{
	int val;
	const char *name;
	va_list ap;

	va_start(ap, attrib);

	switch (attrib) {
	case TETRIS_SET_GHOSTS:
		val = va_arg(ap, int);
		pgame->ghosts = (bool) val;
		break;
	case TETRIS_SET_WALLKICKS:
		val = va_arg(ap, int);
		pgame->wallkicks = (bool) val;
		break;
	case TETRIS_SET_TSPIN:
		val = va_arg(ap, int);
		pgame->tspins = (bool) val;
		break;
	case TETRIS_SET_NAME:
		name = va_arg(ap, const char *);
		strncpy(pgame->id, name, sizeof pgame->id);
		if (sizeof pgame->id > 0)
			pgame->id[sizeof pgame->id -1] = '\0';
		break;
	case TETRIS_SET_DBFILE:
		name = va_arg(ap, const char *);
		strncpy(pgame->db_file, name, sizeof pgame->db_file);
		if (sizeof pgame->db_file > 0)
			pgame->db_file[sizeof pgame->db_file -1] = '\0';
		break;
	default:
		goto err;
	}

	va_end(ap);
	return 1;

err:
	va_end(ap);
	return 0;
}

int tetris_get_attr(tetris *pgame, int attrib, ...)
{
	int *pval;
	int val;
	char *pname;

	va_list ap;
	va_start(ap, attrib);

	switch (attrib) {
	case TETRIS_GET_PAUSED:
		pval = va_arg(ap, int*);
		*pval = (bool) pgame->paused;
		break;
	case TETRIS_GET_WIN:
		pval = va_arg(ap, int*);
		*pval = (bool) pgame->win;
		break;
	case TETRIS_GET_LOSE:
		pval = va_arg(ap, int*);
		*pval = (bool) pgame->lose;
		break;
	case TETRIS_GET_QUIT:
		pval = va_arg(ap, int*);
		*pval = (bool) pgame->quit;
		break;
	case TETRIS_GET_LEVEL:
		pval = va_arg(ap, int*);
		*pval = pgame->level;
		break;
	case TETRIS_GET_LINES:
		pval = va_arg(ap, int*);
		*pval = pgame->lines_destroyed;
		break;
	case TETRIS_GET_SCORE:
		pval = va_arg(ap, int*);
		*pval = pgame->score;
		break;
	case TETRIS_GET_DELAY:
		pval = va_arg(ap, int*);
		*pval = pgame->tick_nsec;
		break;
	case TETRIS_GET_GHOSTS:
		pval = va_arg(ap, int*);
		*pval = (bool) pgame->ghosts;
		break;
	case TETRIS_GET_NAME:
		pname = va_arg(ap, char *);
		val = va_arg(ap, int);

		if (pname == NULL)
			return 0;

		strncpy(pname, pgame->id, val);
		if (val > 0)
			pname[val-1] = '\0';
		break;
	case TETRIS_GET_DBFILE:
		pname = va_arg(ap, char *);
		val = va_arg(ap, int);

		if (pname == NULL)
			return 0;

		strncpy(pname, pgame->db_file, val);
		if (val > 0)
			pname[val-1] = '\0';
		break;
	default:
		goto err;
	}

	va_end(ap);
	return 1;

err:
	va_end(ap);
	return 0;

}

int tetris_get_block(tetris *pgame, int attrib, block *ret)
{
	int len = attrib - TETRIS_BLOCK_HOLD;
	if (len < 0 || len >= TETRIS_NEXT_BLOCKS_LEN+2)
		return 0;

	block *nblock = HOLD_BLOCK(pgame);

	for (; len > 0 && nblock; len--)
		nblock = nblock->entries.le_next;

	if (nblock == NULL)
		return 0;

	memcpy(ret, nblock, sizeof *nblock);

	return 1;
}

int tetris_set_win_condition(tetris *pgame, int (*wc)(tetris *pgame))
{
	pgame->check_win = wc;
	return 1;
}

/* Default game mode, we never win. Game continues until we lose. */
int tetris_classic(tetris *pgame)
{
	(void) pgame;
	return 0;
}


/*******************/
/* Database access */
/*******************/

/* Scores: name, level, score, date */
const char create_scores[] =
	"CREATE TABLE Scores(name TEXT,level INT,score INT,date INT);";

const char insert_scores[] =
	"INSERT INTO Scores VALUES(\"%s\",%d,%d,%lu);";

const char select_scores[] =
	"SELECT * FROM Scores ORDER BY score DESC;";

/* State: name, score, lines, level, date, spaces */
const char create_state[] =
	"CREATE TABLE State(name TEXT,score INT,lines INT,level INT,"
	"date INT,spaces BLOB);";

const char insert_state[] =
	"INSERT INTO State VALUES(\"%s\",%d,%d,%d,%lu,?);";

const char select_state[] =
	"SELECT * FROM State ORDER BY date DESC;";

/* Find the entry we just pulled from the database.
 * There's probably a simpler way than using two SELECT calls, but I'm a total
 * SQL noob, so ... */
const char select_state_rowid[] =
	"SELECT ROWID,date FROM State ORDER BY date DESC;";

/* Remove the entry pulled from the database. This lets us have multiple saves
 * in the database concurently.
 */
const char delete_state_rowid[] =
	"DELETE FROM State WHERE ROWID = ?;";

int tetris_save_score(tetris *pgame)
{
	sqlite3_stmt *stmt;
	char *insert = NULL;
	int len = -1;

	debug("Trying to insert scores to database");

	if (tetris_db_open(pgame) != 1) {
		log_err("Unable to save score.");
		return -1;
	}

	/* Make sure the db has the proper tables */
	sqlite3_prepare_v2(pgame->db_handle, create_scores,
			   sizeof create_scores, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	len = asprintf(&insert, insert_scores,
			pgame->id,
			pgame->level,
			pgame->score,
			time(NULL));

	if (len < 0) {
		log_err("Out of memory");
		return -1;
	} else {
		sqlite3_prepare_v2(pgame->db_handle, insert,
				strlen(insert), &stmt, NULL);
		free(insert);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	tetris_db_close(pgame);

	return 1;
}

int tetris_save_state(tetris *pgame)
{
	sqlite3_stmt *stmt;
	char *insert, *data = NULL;
	int len, ret = 0, data_len = 0;

	debug("Saving game state to database");

	if (tetris_db_open(pgame) != 1) {
		log_err("Unable to save game.");
		return -1;
	}

	/* Create table if it doesn't exist */
	sqlite3_prepare_v2(pgame->db_handle, create_state,
			   sizeof create_state, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	len = asprintf(&insert, insert_state,
		   	pgame->id,
			pgame->score,
			pgame->lines_destroyed,
			pgame->level,
			time(NULL));

	if (len < 0 || insert == NULL) {
		ret = -1;
		log_err("Out of memory");
		goto error;
	}

	data_len = (TETRIS_MAX_ROWS - 2) * sizeof(*pgame->spaces);
	data = malloc(data_len);

	if (data == NULL) {
		/* Non-fatal, we just don't save to database */
		ret = 0;
		log_err("Out of memory");
		goto error;
	}

	memcpy(data, &pgame->spaces[2], data_len);
	sqlite3_prepare_v2(pgame->db_handle, insert, strlen(insert),
			   &stmt, NULL);

	/* NOTE sqlite will free the @data block for us */
	sqlite3_bind_blob(stmt, 1, data, data_len, free);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	ret = 1;

 error:
	if (len > 0)
		free(insert);

	tetris_db_close(pgame);

	return ret;
}

/* Queries database for newest game state information and copies it to pgame.
 */
int tetris_resume_state(tetris *pgame)
{
	sqlite3_stmt *stmt, *delete;
	const char *blob;
	int ret, rowid;

	debug("Trying to restore saved game");

	if (tetris_db_open(pgame) != 1) {
		log_err("Unable to resume game.");
		return -1;
	}

	/* Look for newest entry in table */
	sqlite3_prepare_v2(pgame->db_handle, select_state,
			   sizeof select_state, &stmt, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		if (sqlite3_column_text(stmt, 0))
			strncpy(pgame->id, (const char *)
			sqlite3_column_text(stmt, 0), sizeof pgame->id);
		if (sizeof pgame->id)
			pgame->id[sizeof(pgame->id) -1] = '\0';

		pgame->score		= sqlite3_column_int(stmt, 1);
		pgame->lines_destroyed	= sqlite3_column_int(stmt, 2);
		pgame->level		= sqlite3_column_int(stmt, 3);

		blob			= sqlite3_column_blob(stmt, 5);
		memcpy(&pgame->spaces[2], &blob[0],
		       (TETRIS_MAX_ROWS - 2) * sizeof(*pgame->spaces));

		ret = 1;
	} else {
		log_info("No game saves found");
		ret = 0;
	}

	sqlite3_finalize(stmt);

	sqlite3_prepare_v2(pgame->db_handle, select_state_rowid,
			   sizeof select_state_rowid, &stmt, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		rowid = sqlite3_column_int(stmt, 0);

		/* delete from table */
		sqlite3_prepare_v2(pgame->db_handle, delete_state_rowid,
				   sizeof delete_state_rowid, &delete, NULL);

		sqlite3_bind_int(delete, 1, rowid);
		sqlite3_step(delete);
		sqlite3_finalize(delete);
	}

	sqlite3_finalize(stmt);
	tetris_db_close(pgame);

	return ret;
}

/* Returns a pointer to the first element of a tail queue, of @n length.
 * This list can be iterated over to extract the fields:
 * name, level, score, date.
 */
int tetris_get_scores(tetris *pgame, struct tetris_score_head **res, size_t n)
{
	sqlite3_stmt *stmt;

	debug("Getting highscores");

	if (tetris_db_open(pgame) != 1) {
		log_err("Unable to get highscores.");
		return -1;
	}

	sqlite3_prepare_v2(pgame->db_handle, select_scores,
		sizeof select_scores, &stmt, NULL);

	while (n-- > 0 && sqlite3_step(stmt) == SQLITE_ROW) {
		if (sqlite3_column_count(stmt) != 4)
			/* improperly formatted row */
			break;

		tetris_score *np = malloc(sizeof *np);
		if (!np) {
			log_warn("Out of memory");
			break;
		}

		memset(np->id, 0, sizeof np->id);
		strncpy(np->id, (const char *)
			sqlite3_column_text(stmt, 0), sizeof np->id);

		if (sizeof np->id > 0)
			np->id[sizeof np->id -1] = '\0';

		np->level	= sqlite3_column_int(stmt, 1);
		np->score	= sqlite3_column_int(stmt, 2);
		np->date	= sqlite3_column_int(stmt, 3);

		if (!np->score || !np->date) {
			free(np);
			continue;
		}

		TAILQ_INSERT_TAIL(&pgame->score_head, np, entries);
	}

	sqlite3_finalize(stmt);

	*res = &pgame->score_head;

	tetris_db_close(pgame);

	return 1;
}

void tetris_clean_scores(tetris *pgame)
{
	while (pgame->score_head.tqh_first) {
		tetris_score *tmp = pgame->score_head.tqh_first;
		TAILQ_REMOVE(&pgame->score_head, tmp, entries);
		free(tmp);
	}
}

/* Draw the Hold and Next blocks listing on the side of the game. */
int tetris_draw_pieces(tetris *pgame, WINDOW *scr)
{
	const block *pblock;
	size_t i, count = 0;

	werase(scr);
	wattrset(scr, COLOR_PAIR(1));
	box(scr, 0, 0);

	pblock = HOLD_BLOCK(pgame);

	for (i = 0; i < LEN(pblock->p); i++) {
		wattrset(scr, A_BOLD |
			COLOR_PAIR((pblock->type % COLORS_LENGTH) +1));

		mvwadd_wch(scr, pblock->p[i].y +2,
				pblock->p[i].x +3, PIECES_CHAR);
	}

	pblock = FIRST_NEXT_BLOCK(pgame);

	while (pblock) {
		for (i = 0; i < LEN(pblock->p); i++) {
			wattrset(scr, A_BOLD |
				COLOR_PAIR((pblock->type % COLORS_LENGTH) +1));

			mvwadd_wch(scr, pblock->p[i].y +2 +(count*3),
					pblock->p[i].x +9, PIECES_CHAR);
		}

		count++;
		pblock = pblock->entries.le_next;
	}

	wnoutrefresh(scr);

	return 1;
}

int tetris_draw_board(tetris *pgame, WINDOW *scr)
{
	size_t i;
	block *pblock;

	werase(scr);

	/* Draw board outline */
	wattrset(scr, A_BOLD | COLOR_PAIR(5));

	mvwvline(scr, 0, 0, '*', TETRIS_MAX_ROWS -1);
	mvwvline(scr, 0, TETRIS_MAX_COLUMNS +1, '*', TETRIS_MAX_ROWS -1);

	mvwhline(scr, TETRIS_MAX_ROWS -2, 0, '*', TETRIS_MAX_COLUMNS +2);

	/* Draw the background of the board. Dot every other column */
	wattrset(scr, COLOR_PAIR(1));
	for (i = 2; i < TETRIS_MAX_ROWS; i++)
		mvwprintw(scr, i -2, 1, " . . . . .");

	/* Draw the ghost block */
	pblock = pgame->ghost;
	wattrset(scr, A_DIM |
		COLOR_PAIR((pblock->type % COLORS_LENGTH) +1));

	for (i = 0; i < LEN(pblock->p); i++) {
		mvwadd_wch(scr,
		    pblock->p[i].y +pblock->row_off -2,
		    pblock->p[i].x +pblock->col_off +1,
		    PIECES_CHAR);
	}

	/* Draw the game board, minus the two hidden rows above the game */
	for (i = 2; i < TETRIS_MAX_ROWS; i++) {
		if (pgame->spaces[i] == 0)
			continue;

		size_t j;
		for (j = 0; j < TETRIS_MAX_COLUMNS; j++) {
			if (!tetris_at_yx(pgame, i, j))
				continue;

			wattrset(scr, A_BOLD |
				COLOR_PAIR(pgame->colors[i][j] +1));
			mvwadd_wch(scr, i -2, j +1, PIECES_CHAR);
		}
	}

	/* Draw the falling block to the board */
	pblock = CURRENT_BLOCK(pgame);
	for (i = 0; i < LEN(pblock->p); i++) {
		wattrset(scr, A_DIM |
			COLOR_PAIR((pblock->type % COLORS_LENGTH) +1));

		mvwadd_wch(scr,
		    pblock->p[i].y +pblock->row_off -2,
		    pblock->p[i].x +pblock->col_off +1,
		    PIECES_CHAR);
	}

	/* Draw "PAUSED" text */
	if (pgame->paused) {
		wattrset(scr, A_BOLD | COLOR_PAIR(1));
		mvwprintw(scr, (TETRIS_MAX_ROWS -2) /2 -1,
				 (TETRIS_MAX_COLUMNS -2) /2 -1,
				 "PAUSED");
	}

	/* Draw username */
	wattrset(scr, COLOR_PAIR(1));
	mvwprintw(scr, BOARD_HEIGHT -1, (BOARD_WIDTH -strlen(pgame->id))/2,
		"%s", pgame->id);

	wnoutrefresh(scr);
	return 1;
}

/************************************/
/*   End Public interface to game   */
/************************************/
