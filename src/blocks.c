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

#include <assert.h>
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

/* three blocks,
 * current falling block,
 * next block to fall,
 * hold block
 */
static struct blocks blocks[3];

/*
 * randomizes block and sets the initial positions of the pieces
 */
static void randomize_block(struct blocks_game *pgame, struct blocks *block)
{
	int index = 0; /* used in macro def. PIECE_XY() */

	if (!pgame || !block)
		return;

	if (bag_is_empty())
		bag_random_generator();

	block->type = bag_next_piece();

	/* Try to center the block on the board */
	block->col_off = pgame->width / 2;
	block->row_off = 1;

	block->soft_drop = 0;
	block->hard_drop = 0;

	/* Get a consistent color for each block. We don't know what it is.
	 * screen.c decides in what order colors are defined */
	block->color = block->type;

	/* The piece at (0, 0) is the pivot when we rotate */
	switch (block->type) {
	case SQUARE_BLOCK:
		PIECE_XY(-1, -1);
		PIECE_XY( 0, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		break;
	case LINE_BLOCK:
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
	case L_REV_BLOCK:
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
	case Z_REV_BLOCK:
		PIECE_XY( 0, -1);
		PIECE_XY( 1, -1);
		PIECE_XY(-1,  0);
		PIECE_XY( 0,  0);
		break;
	default:
		log_err("Piece not found: %d\n", block->type);
		exit(EXIT_FAILURE);
	}
}

/* Current block hit the bottom of the game, remove it.
 * Swap the "next block" with "current block", and randomize next block.
 */
static inline void update_cur_next(struct blocks_game *pgame)
{
	struct blocks *old;

	if (!pgame)
		return;

	old = pgame->cur;

	pgame->cur = pgame->next;
	pgame->next = old;

	randomize_block(pgame, pgame->next);
}

/* rotate pieces in blocks by either 90^ or -90^ around (0, 0) pivot */
static int rotate_block(struct blocks_game *pgame, struct blocks *block,
			enum blocks_input_cmd cmd)
{
	int mod = 1;
	size_t i;

	if (!pgame || !block)
		return -1;

	if (block->type == SQUARE_BLOCK)
		return 1;

	if (cmd == ROT_LEFT)
		mod = -1;

	/* Check each piece for a collision before we write any changes */
	/* META: GCC complains about checks betwen signed and
	 * unsigned types. sizeof 'returns' type size_t(unsigned int). So just
	 * use size_t in most loops for consistency.
	 */
	for (i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
		bounds_x = block->p[i].y * (-mod) + block->col_off;
		bounds_y = block->p[i].x * (mod) + block->row_off;

		/* Check for out of bounds on each piece */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    /* Also check if a piece already exists here */
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return 0;
	}

	/* No collisions, so update the block position. */
	for (i = 0; i < LEN(block->p); i++) {
		int new_x, new_y;
		new_x = block->p[i].y * (-mod);
		new_y = block->p[i].x * (mod);

		block->p[i].x = new_x;
		block->p[i].y = new_y;
	}

	return 1;
}

/* translate pieces in block horizontally. */
static int translate_block(struct blocks_game *pgame, struct blocks *block,
			   enum blocks_input_cmd cmd)
{
	int dir = 1;
	size_t i;

	if (!pgame || !block)
		return -1;

	if (cmd == MOVE_LEFT)
		dir = -1;

	/* Check each piece for a collision */
	for (i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
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

static inline void update_tick_speed(struct blocks_game *pgame)
{
	double speed = 1.0f;

	if (!pgame)
		return;

	/* See tests/level-curve.c */
	speed += atan((double)pgame->level / 5.0) * 2 / PI * 3;
	pgame->nsec = (uint32_t) ((double)1E9 / speed) - 1;
}

static int destroy_lines(struct blocks_game *pgame)
{
	uint32_t full_row;
	uint8_t destroyed;
	size_t i, j;

	/* point modifier, difficult clears earn more over time.
	 * a tetris (4 line clears) counts for 1 difficult move.
	 *
	 * difficult values >0 boost points by 3/2
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
	destroyed = 0;

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

		/* The game does not function if for some reason this fails */
		assert(pgame->spaces[i] <= full_row);

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

	switch (destroyed) {
		case 1:
			difficult = 0;
			point_mod = 100;
			break;
		case 2:
			difficult = 0;
			point_mod = 300;
			break;
		case 3:
			difficult = 0;
			point_mod = 500;
			break;
		case 4:
			if (++difficult > 1)
				point_mod = 1200;
			else
				point_mod = 800;
			break;
	}

	pgame->score += point_mod * pgame->level + pgame->cur->soft_drop
			+ (pgame->cur->hard_drop * 2);

	return destroyed;
}

/* removes the block from the board */
static void unwrite_piece(struct blocks_game *pgame, struct blocks *block)
{
	size_t i, x, y;

	if (!pgame || !block)
		return;

	for (i = 0; i < LEN(block->p); i++) {
		y = block->row_off + block->p[i].y;
		x = block->col_off + block->p[i].x;

		/* Remove the bit where the block exists */
		pgame->spaces[y] &= ~(1 << x);
		pgame->colors[y][x] = 0;
	}
}

/* writes the piece to the board, checking bounds and collisions */
static void write_piece(struct blocks_game *pgame, struct blocks *block)
{
	int px[4], py[4];
	size_t i;

	if (!pgame || !block)
		return;

	for (i = 0; i < LEN(pgame->cur->p); i++) {
		py[i] = block->row_off + block->p[i].y;
		px[i] = block->col_off + block->p[i].x;

		if (px[i] < 0 || px[i] >= pgame->width ||
		    py[i] < 0 || py[i] >= pgame->height)
			return;
	}

	/* pgame->spaces is an array of bit fields, 1 per row */
	for (i = 0; i < LEN(pgame->cur->p); i++) {
		/* Set the bit where the block exists */
		pgame->spaces[py[i]] |= (1 << px[i]);
		pgame->colors[py[i]][px[i]] = block->color;
	}
}

/* tries to advance the block one space down */
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

	/* increment the row offset moves the block down one space */
	block->row_off++;

	return 1;
}

int blocks_init(struct blocks_game *pgame)
{
	size_t i;

	if (!pgame)
		return -1;

	log_info("Initializing game data");
	memset(pgame, 0, sizeof *pgame);

	pthread_mutex_init(&pgame->lock, NULL);

	/* Default dimensions, modified in screen_draw_menu()
	 * We assume the board is the max size (12x24). If the user wants a
	 * smaller board, we use only the smaller pieces.
	 */
	pgame->width = BLOCKS_MAX_COLUMNS;
	pgame->height = BLOCKS_MAX_ROWS;

	pgame->level = 1;
	pgame->nsec = 1E9 - 1;
	pgame->pause_ticks = 1E3;

	/* randomize the initial blocks */
	for (i = 0; i < LEN(blocks); i++) {
		randomize_block(pgame, &blocks[i]);
		/* Start with random color, so cur and next don't follow each
		 * other.
		 */
		blocks[i].color = rand();
	}

	pgame->cur = &blocks[0];
	pgame->next = &blocks[1];
	pgame->hold = NULL;

	/* Allocate the maximum size necessary to accomodate the
	 * largest board size */
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

int blocks_cleanup(struct blocks_game *pgame)
{
	size_t i;

	if (!pgame)
		return -1;

	log_info("Cleaning game data");
	pthread_mutex_destroy(&pgame->lock);

	/* Free all the memory allocated */
	for (i = 0; i < BLOCKS_MAX_ROWS; i++)
		free(pgame->colors[i]);

	return 1;
}

/*
 * These two functions are separate threads. Game operations in here are
 * unsafe.
 */

/*
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

		if (pgame->pause && pgame->pause_ticks) {
			pgame->pause_ticks--;
			continue;
		}

		/* Unpause the game if we're out of pause ticks */
		pgame->pause = (pgame->pause && pgame->pause_ticks);

		if (pgame->lose || pgame->quit)
			break;

		pthread_mutex_lock(&pgame->lock);

		unwrite_piece(pgame, pgame->cur);
		hit = drop_block(pgame, pgame->cur);
		write_piece(pgame, pgame->cur);

		if (hit == 0) {
			destroy_lines(pgame);
			update_cur_next(pgame);
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
	unwrite_piece(pgame, pgame->cur);

	return NULL;
}

void *blocks_input(void *vp)
{
	struct blocks_game *pgame = vp;
	struct blocks *tmp;
	int ch;

	if (!pgame || !pgame->cur)
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
		unwrite_piece(pgame, pgame->cur);

		/* modify it */
		switch (toupper(ch)) {
		case 'A':
			translate_block(pgame, pgame->cur, MOVE_LEFT);
			break;
		case 'D':
			translate_block(pgame, pgame->cur, MOVE_RIGHT);
			break;
		case 'S':
			if (drop_block(pgame, pgame->cur))
				pgame->cur->soft_drop++;
			break;
		case 'W':
			/* drop the block to the bottom of the game */
			while (drop_block(pgame, pgame->cur))
				pgame->cur->hard_drop++;
			//pgame->lock_delay = 0;
			break;
		case 'Q':
			if (!rotate_block(pgame, pgame->cur, ROT_LEFT)) {
//				try_wall_kick(pgame, pgame->cur, ROT_LEFT);
				;
			}
			break;
		case 'E':
			if (!rotate_block(pgame, pgame->cur, ROT_RIGHT)) {
//				try_wall_kick(pgame, pgame->cur, ROT_RIGHT);
				;
			}
			break;
		case ' ':{
				if (pgame->hold == NULL) {
					pgame->hold = &blocks[2];
				}

				tmp = pgame->hold;

				pgame->hold = pgame->next;
				pgame->next = tmp;
				break;
			}
		}

		/* then rewrite it */
		write_piece(pgame, pgame->cur);

 draw:
		screen_draw_game(pgame);
		pthread_mutex_unlock(&pgame->lock);
	}

	return NULL;
}
