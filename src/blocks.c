#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"
#include "screen.h"

static struct block blocks[3];

#define NUM_BLOCKS 7
enum {
	SQUARE_BLOCK,
	LINE_BLOCK,
	T_BLOCK,
	L_BLOCK,
	L_REV_BLOCK,
	Z_BLOCK,
	Z_REV_BLOCK
};

/*
 * creates a random block and sets the initial positions of the pieces
 * This is the most called function in the file (that adds/removes memory).
 */
static void
create_block (struct block_game *pgame, struct block *block)
{
	block->type = rand() %NUM_BLOCKS;
	block->col_off = pgame->width/2;
	block->row_off = 1;
	block->color++;

	/* The piece at (0, 0) is the pivot when we rotate */
	switch (block->type) {
	case SQUARE_BLOCK:
		block->p[0].x = -1;	block->p[0].y = -1;
		block->p[1].x = 0;	block->p[1].y = -1;
		block->p[2].x = -1;	block->p[2].y = 0;
		block->p[3].x = 0;	block->p[3].y = 0;
		break;
	case LINE_BLOCK:
		block->col_off--; // center
		block->p[0].x = -1;	block->p[0].y = 0;
		block->p[1].x = 0;	block->p[1].y = 0;
		block->p[2].x = 1;	block->p[2].y = 0;
		block->p[3].x = 2;	block->p[3].y = 0;
		break;
	case T_BLOCK:
		block->p[0].x = 0;	block->p[0].y = -1;
		block->p[1].x = -1;	block->p[1].y = 0;
		block->p[2].x = 0;	block->p[2].y = 0;
		block->p[3].x = 1;	block->p[3].y = 0;
		break;
	case L_BLOCK:
		block->p[0].x = 1;	block->p[0].y = -1;
		block->p[1].x = -1;	block->p[1].y = 0;
		block->p[2].x = 0;	block->p[2].y = 0;
		block->p[3].x = 1;	block->p[3].y = 0;
		break;
	case L_REV_BLOCK:
		block->p[0].x = -1;	block->p[0].y = -1;
		block->p[1].x = -1;	block->p[1].y = 0;
		block->p[2].x = 0;	block->p[2].y = 0;
		block->p[3].x = 1;	block->p[3].y = 0;
		break;
	case Z_BLOCK:
		block->p[0].x = -1;	block->p[0].y = -1;
		block->p[1].x = 0;	block->p[1].y = -1;
		block->p[2].x = 0;	block->p[2].y = 0;
		block->p[3].x = 1;	block->p[3].y = 0;
		break;
	case Z_REV_BLOCK:
		block->p[0].x = 0;	block->p[0].y = -1;
		block->p[1].x = 1;	block->p[1].y = -1;
		block->p[2].x = -1;	block->p[2].y = 0;
		block->p[3].x = 0;	block->p[3].y = 0;
		break;
	}
}

/* Current block hit the bottom of the game, remove it.
 * Then make the next block the current block, and create a random next block
 */
static void
update_cur_next (struct block_game *pgame)
{
	struct block *old = pgame->cur;

	pgame->cur = pgame->next;
	pgame->next = old;

	create_block (pgame, pgame->next);
}

/*
 * rotate blocks by either 90^ or -90^.
 */
static int
rotate_block (struct block_game *pgame, struct block *block, enum block_cmd cmd)
{
	if (!pgame || !block)
		return -1;

	if (block->type == SQUARE_BLOCK)
		return 1;

	int py[4], px[4], mod = 1;
	if (cmd == ROT_LEFT)
		mod = -1;

	for (size_t i = 0; i < LEN(block->p); i++) {
		/* Rotate each piece */
		px[i] = block->p[i].y * (-mod);
		py[i] = block->p[i].x * mod;

		int bounds_x, bounds_y;
		bounds_x = px[i] + block->col_off;
		bounds_y = py[i] + block->row_off;

		/* Check out of bounds on each block */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return -1;
	}

	/* No collisions, update block position */
	for (int i = 0; i < 4; i++) {
		block->p[i].x = px[i];
		block->p[i].y = py[i];
	}

	return 1;
}

/*
 * translate horizontally to the right or left.
 */
static int
translate_block (struct block_game *pg, struct block *block, enum block_cmd cmd)
{
	int dir = 1;
	if (cmd == MOVE_LEFT)
		dir = -1;

	for (size_t i = 0; i < LEN(block->p); i++) {

		int bounds_x, bounds_y;
		bounds_x = block->p[i].x + block->col_off + dir;
		bounds_y = block->p[i].y + block->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= pg->width ||
		    bounds_y < 0 || bounds_y >= pg->height ||
		    blocks_at_yx(pg, bounds_y, bounds_x))
			return -1;
	}

	block->col_off += dir;

	return 1;
}

static void
update_tick_speed (struct block_game *pgame)
{
	/* See tests/level-curve.c */
	double speed;
	speed = atan (pgame->level/(double)5) * (pgame->mod+1) +1;
	pgame->nsec = (int) ((double)1E9/speed);
}

static int
destroy_lines (struct block_game *pgame)
{
	uint8_t destroyed = 0;

	/* Check first two rows for any block */
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < pgame->width; j++)
			if (blocks_at_yx(pgame, i, j))
				pgame->loss = true;

	/* Check each row for a full line of blocks */
	for (int i = pgame->height-1; i >= 2; i--) {

		int j = 0;
		for (; j < pgame->width; j++)
			if (!blocks_at_yx(pgame, i, j))
				break;

		if (j != pgame->width)
			continue;

		debug ("Removed line %2d", i+1);
		pgame->spaces[i] = 0;

		destroyed++;

		/* Move everything down */
		for (int k = i; k > 0; k--)
			pgame->spaces[k] = pgame->spaces[k-1];

		i++; // recheck this one
	}

	/* Update player level and game speed if necessary */
	for (int i = destroyed; i > 0; i--) {

		pgame->lines_destroyed++;
		pgame->score += pgame->level * (pgame->mod+1);

		if (pgame->lines_destroyed < (pgame->level *2 + 5))
			continue;
		pgame->lines_destroyed = 0;
		pgame->level++;

		update_tick_speed (pgame);
	}

	return destroyed;
}

static void
unwrite_piece (struct block_game *pgame, struct block *block)
{
	if (pgame == NULL || block == NULL)
		return;

	for (size_t i = 0; i < LEN(block->p); i++) {
		int y, x;
		y = block->row_off + block->p[i].y;
		x = block->col_off + block->p[i].x;

		/* Remove the bit where the block exists */
		pgame->spaces[y] &= ~(1 << x);
		pgame->colors[y][x] = 0;
	}
}

static void
write_piece (struct block_game *pgame, struct block *block)
{
	if (pgame == NULL || block == NULL)
		return;

	int px[4], py[4];

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		py[i] = block->row_off + block->p[i].y;
		px[i] = block->col_off + block->p[i].x;

		if (px[i] < 0 || px[i] >= pgame->width ||
		    py[i] < 0 || py[i] >= pgame->height)
			return;
	}

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		/* Set the bit where the block exists */
		pgame->spaces[py[i]] |= (1 << px[i]);
		pgame->colors[py[i]][px[i]] = block->color;
	}
}

/* returns true if something is below the block.
 * else advance the block once block down
 */
static bool
drop_block (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return 0;

	for (int i = 0; i < 4; i++) {
		int bounds_y, bounds_x;
		bounds_y = pgame->cur->p[i].y + pgame->cur->row_off +1;
		bounds_x = pgame->cur->p[i].x + pgame->cur->col_off;

		if (bounds_y >= pgame->height ||
		    blocks_at_yx (pgame, bounds_y, bounds_x))
			return true;
	}

	pgame->cur->row_off++;
	return false;
}

/*
 * Game is over when this function returns.
 */
int
blocks_loop (struct block_game *pgame)
{
	if (pgame == NULL)
		return -1;

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 0;

	update_tick_speed (pgame);

	for (;;) {
		ts.tv_nsec = pgame->nsec;
		nanosleep (&ts, NULL);

		if (pgame->pause)
			continue;

		if (pgame->loss || pgame->quit)
			break;

		pthread_mutex_lock (&pgame->lock);

		unwrite_piece (pgame, pgame->cur);
		bool hit = drop_block (pgame);
		write_piece (pgame, pgame->cur);
	
		if (hit) {
			update_cur_next (pgame);
			destroy_lines (pgame);
		}

		screen_draw_game (pgame);

		pthread_mutex_unlock (&pgame->lock);
	}

	unwrite_piece (pgame, pgame->cur);

	return 1;
}

int
blocks_init (struct block_game *pgame)
{
	if (pgame == NULL)
		return -1;

	log_info ("Initializing game data");
	memset (pgame, 0, sizeof *pgame);

	pthread_mutex_init (&pgame->lock, NULL);

	/* Default dimensions, modified in screen_draw_menu()
	 * We assume the board is the max size (12x24). If the user wants a
	 * smaller board, we only use the smaller pieces
	 */
	pgame->width = BLOCKS_COLUMNS;
	pgame->height = BLOCKS_ROWS;

	/* Default difficulty, modified in screen_draw_menu() */
	pgame->mod = DIFF_NORMAL;

	pgame->level = 1;
	pgame->nsec = 1E9 -1;

	/* Initialize the blocks */
	for (size_t i = 0; i < LEN(blocks); i++) {
		create_block (pgame, &blocks[i]);
		blocks[i].color = rand(); // Start with random color
	}

	pgame->cur = &blocks[0];
	pgame->next = &blocks[1];
	pgame->save = NULL;

	/* Allocate the maximum size necessary to accomodate the
	 * largest board size */
	for (int i = 0; i < BLOCKS_ROWS; i++) {
		pgame->colors[i] = calloc (BLOCKS_COLUMNS,
				sizeof (*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err ("Out of memory");
			exit (2);
		}
	}

	return 1;
}

int
blocks_move (struct block_game *pgame, enum block_cmd cmd)
{
	if (!pgame || !pgame->cur)
		return -1;

	pthread_mutex_lock (&pgame->lock);

	if (pgame->pause || pgame->quit)
		goto draw;

	unwrite_piece (pgame, pgame->cur);

	if (cmd == MOVE_LEFT || cmd == MOVE_RIGHT) {
		translate_block (pgame, pgame->cur, cmd);

	} else if (cmd == MOVE_DOWN) {
		drop_block (pgame);

	} else if (cmd == MOVE_DROP) {
		while (!drop_block (pgame));
	}

	/* Swap next and save pieces */
	else if (cmd == SAVE_PIECE) {
		if (pgame->save == NULL) {
			pgame->save = &blocks[2];
		}

		struct block *tmp = pgame->save;

		pgame->save = pgame->next;
		pgame->next = tmp;
	}

	/* XXX This is horribly hacky, and only works if you rotate LEFT on the
	 * RIGHT wall, and RIGHT on the LEFT wall. Make this better
	 */

	/* We implement wall kicks */
	else if (cmd == ROT_LEFT || cmd == ROT_RIGHT) {

		struct block tmp;
		memcpy (&tmp, pgame->cur, sizeof tmp);

		/* Try to rotate the block; if it works, commit and quit. */
		if (rotate_block (pgame, &tmp, cmd) > 0)
			memcpy (pgame->cur, &tmp, sizeof tmp);

		/* Try to move the block */
		else {
			if (cmd == ROT_LEFT)
				translate_block (pgame, &tmp, MOVE_RIGHT);
			else
				translate_block (pgame, &tmp, MOVE_LEFT);

			/* Try again to rotate */
			if (rotate_block (pgame, &tmp, cmd) > 0)
				memcpy (pgame->cur, &tmp, sizeof tmp);
		}
	}

	write_piece (pgame, pgame->cur);

draw:
	screen_draw_game (pgame);
	pthread_mutex_unlock (&pgame->lock);

	return 1;
}

int
blocks_cleanup (struct block_game *pgame)
{
	if (pgame == NULL)
		return -1;

	log_info ("Cleaning game data");
	pthread_mutex_destroy (&pgame->lock);

	/* Don't use board size; free all the memory allocated */
	for (int i = 0; i < BLOCKS_ROWS; i++)
		free (pgame->colors[i]);

	return 1;
}
