#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"
#include "screen.h"

static void create_block (struct block **);
static void destroy_block (struct block **);
static int destroy_lines (struct block_game *);
static bool drop_block (struct block_game *);
static int rotate_block (struct block_game *, enum block_cmd);
static int translate_block (struct block_game *, enum block_cmd);
static void unwrite_cur_piece (struct block_game *);
static void update_tick_speed (struct block_game *);
static void write_cur_piece (struct block_game *);

static void
destroy_block (struct block **dest)
{
	free (*dest);
	*dest = NULL;
}

/*
 * create_block
 * creates a new random (evenly distributed) block
 * and sets the initial positions of the pieces
 */
static void
create_block (struct block **new_block)
{
	static int val = 0;
	struct block *pnb = calloc (1, sizeof *pnb);

	if (pnb == NULL) {
		log_err ("Out of memory, %lu", sizeof *pnb);
		exit (2);
	}

	enum block_type blocks[] = {
		[SQUARE_BLOCK]	= SQUARE_BLOCK,
		[LINE_BLOCK]	= LINE_BLOCK,
		[T_BLOCK]	= T_BLOCK,
		[L_BLOCK]	= L_BLOCK,
		[L_REV_BLOCK]	= L_REV_BLOCK,
		[Z_BLOCK]	= Z_BLOCK,
		[Z_REV_BLOCK]	= Z_REV_BLOCK,
	};

	pnb->type = rand () % LEN(blocks);
	pnb->col_off = BLOCKS_COLUMNS/2;
	pnb->row_off = 1;
	pnb->color = val++;

	/* The position at (0, 0) is the pivot when we rotate */
	if (pnb->type == SQUARE_BLOCK) {
		pnb->p[0].x = -1; pnb->p[0].y = -1;
		pnb->p[1].x = 0; pnb->p[1].y = -1;
		pnb->p[2].x = -1; pnb->p[2].y = 0;
		pnb->p[3].x = 0; pnb->p[3].y = 0;
	} else if (pnb->type == LINE_BLOCK) {
		pnb->col_off--; // center
		pnb->p[0].x = -1; pnb->p[0].y = 0;
		pnb->p[1].x = 0; pnb->p[1].y = 0;
		pnb->p[2].x = 1; pnb->p[2].y = 0;
		pnb->p[3].x = 2; pnb->p[3].y = 0;
	} else if (pnb->type == T_BLOCK) {
		pnb->p[0].x = 0; pnb->p[0].y = -1;
		pnb->p[1].x = -1; pnb->p[1].y = 0;
		pnb->p[2].x = 0; pnb->p[2].y = 0;
		pnb->p[3].x = 1; pnb->p[3].y = 0;
	} else if (pnb->type == L_BLOCK) {
		pnb->p[0].x = 1; pnb->p[0].y = -1;
		pnb->p[1].x = -1; pnb->p[1].y = 0;
		pnb->p[2].x = 0; pnb->p[2].y = 0;
		pnb->p[3].x = 1; pnb->p[3].y = 0;
	} else if (pnb->type == L_REV_BLOCK) {
		pnb->p[0].x = -1; pnb->p[0].y = -1;
		pnb->p[1].x = -1; pnb->p[1].y = 0;
		pnb->p[2].x = 0; pnb->p[2].y = 0;
		pnb->p[3].x = 1; pnb->p[3].y = 0;
	} else if (pnb->type == Z_BLOCK) {
		pnb->p[0].x = -1; pnb->p[0].y = -1;
		pnb->p[1].x = 0; pnb->p[1].y = -1;
		pnb->p[2].x = 0; pnb->p[2].y = 0;
		pnb->p[3].x = 1; pnb->p[3].y = 0;
	} else if (pnb->type == Z_REV_BLOCK) {
		pnb->p[0].x = 0; pnb->p[0].y = -1;
		pnb->p[1].x = 1; pnb->p[1].y = -1;
		pnb->p[2].x = -1; pnb->p[2].y = 0;
		pnb->p[3].x = 0; pnb->p[3].y = 0;
	} else
		goto error;

	*new_block = pnb;
	return;
error:
	log_err ("Block type (%d) does not exist", pnb->type);
	destroy_block (new_block);
}

/*
 * rotate blocks
 * move_blocks locks the game mutex, so we can be sure the blocks won't update
 * while we're doing this
 */
static int
rotate_block (struct block_game *pgame, enum block_cmd cmd)
{
	if (pgame->cur->type == SQUARE_BLOCK)
		return 1;

	int py[4], px[4], mod;
	mod = (cmd == ROT_LEFT) ? -1 : 1;

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		/* Rotate each piece */
		px[i] = pgame->cur->p[i].y * -1 * mod;
		py[i] = pgame->cur->p[i].x * mod;

		/* Check out of bounds before we write it */
		int bounds_x, bounds_y;
		bounds_x = px[i] + pgame->cur->col_off;
		bounds_y = py[i] + pgame->cur->row_off;

		if (bounds_x < 0 || bounds_x >= BLOCKS_COLUMNS ||
		    bounds_y < 0 || bounds_y >= BLOCKS_ROWS ||
		    pgame->spaces[bounds_y][bounds_x])
			return -1;
	}

	for (int i = 0; i < 4; i++) {
		pgame->cur->p[i].x = px[i];
		pgame->cur->p[i].y = py[i];
	}

	return 1;
}

/*
 * translate horizontally
 * move_blocks locks the game mutex, so we can be sure the blocks won't update
 * while we're doing this
 */
static int
translate_block (struct block_game *pgame, enum block_cmd cmd)
{
	int dir;
	dir = (cmd == MOVE_LEFT) ? -1 : 1;

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		int bounds_x, bounds_y;

		bounds_x = pgame->cur->p[i].x + pgame->cur->col_off + dir;
		bounds_y = pgame->cur->p[i].y + pgame->cur->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= BLOCKS_COLUMNS ||
		    bounds_y < 0 || bounds_y >= BLOCKS_ROWS ||
		    pgame->spaces[bounds_y][bounds_x])
			return -1;
	}

	pgame->cur->col_off += dir;

	return 1;
}

static void
update_tick_speed (struct block_game *pgame)
{
	/* See tests/level-curve.c */
	double speed;
	speed = atan (pgame->level/(double)10) *pgame->mod * 2/PI +1;
	pgame->nsec = (int) ((double)1E9/speed);
}

static int
destroy_lines (struct block_game *pgame)
{
	uint8_t destroyed = 0;

	/* Check first two rows for any block */
	for (int i = 0; i < 2; i++)
		for (int j = 0; j < BLOCKS_COLUMNS; j++)
			if (pgame->spaces[i][j])
				pgame->loss = true;

	/* Check each row for a full line of blocks */
	for (int i = BLOCKS_ROWS-1; i >= 2; i--) {

		int j = 0;
		for (; j < BLOCKS_COLUMNS; j++)
			if (pgame->spaces[i][j] == false)
				break;
		if (j != BLOCKS_COLUMNS)
			continue;

		/* Full line found */
		free (pgame->spaces[i]);
		log_info ("Removed line %2d", i-1);
		destroyed++;

		/* Move everything down */
		for (int k = i; k > 0; k--)
			pgame->spaces[k] = pgame->spaces[k-1];

		/* Add new empty row to top of game */
		pgame->spaces[0] = calloc (BLOCKS_COLUMNS, sizeof (bool));
		if (pgame->spaces[0] == NULL) {
			log_err ("Out of memory, %lu", BLOCKS_COLUMNS);
			exit (2);
		}

		i++; // recheck this one
	}

	/* Update player level and game speed if necessary */
	for (int i = destroyed; i > 0; i--) {

		pgame->lines_destroyed++;
		pgame->score += pgame->level * pgame->mod;

		if (pgame->lines_destroyed < pgame->level)
			continue;
		pgame->lines_destroyed = 0;
		pgame->level++;

		update_tick_speed (pgame);
	}

	return destroyed;
}

static void
unwrite_cur_piece (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return;

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		int y, x;
		y = pgame->cur->row_off + pgame->cur->p[i].y;
		x = pgame->cur->col_off + pgame->cur->p[i].x; 

		pgame->spaces[y][x] = 0;
		pgame->colors[y][x] = 0;
	}
}

static void
write_cur_piece (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return;

	int px[4], py[4];

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		py[i] = pgame->cur->row_off + pgame->cur->p[i].y;
		px[i] = pgame->cur->col_off + pgame->cur->p[i].x; 

		if (py[i] < 0 || py[i] >= BLOCKS_ROWS ||
		    px[i] < 0 || px[i] >= BLOCKS_COLUMNS)
			return;
	}

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		pgame->spaces[py[i]][px[i]] = 1;
		pgame->colors[py[i]][px[i]] = pgame->cur->color;
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

		if (bounds_y >= BLOCKS_ROWS ||
		    pgame->spaces[bounds_y][bounds_x])
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

	for (;;) {
		update_tick_speed (pgame);
		ts.tv_nsec = pgame->nsec;

		nanosleep (&ts, NULL);

		if (pgame->pause)
			continue;

		if (pgame->loss || pgame->quit) {
			unwrite_cur_piece (pgame);
			break;
		}

		pthread_mutex_lock (&pgame->lock);

		unwrite_cur_piece (pgame);
		bool hit = drop_block (pgame);
		write_cur_piece (pgame);
	
		if (hit) {
			destroy_block (&pgame->cur);
			pgame->cur = pgame->next;
			create_block (&pgame->next);
			destroy_lines (pgame);
		}

		pthread_mutex_unlock (&pgame->lock);

		screen_draw_game (pgame);
	}

	return 0;
}

int
blocks_init (struct block_game *pgame)
{
	if (pgame == NULL)
		return -1;

	log_info ("Initializing game data");
	srand (time (NULL));

	memset (pgame, 0, sizeof *pgame);
	pgame->level = 1;
	pgame->mod = DIFF_NORMAL;
	pgame->nsec = 1E9 -1;

	create_block (&pgame->cur);
	create_block (&pgame->next);

	for (int i = 0; i < BLOCKS_ROWS; i++) {
		pgame->spaces[i] = calloc (BLOCKS_COLUMNS, 1);
		if (!pgame->spaces[i]) {
			log_err ("Out of memory, %lu", BLOCKS_COLUMNS);
			exit (2);
		}

		pgame->colors[i] = calloc (BLOCKS_COLUMNS, 1);
		if (!pgame->colors[i]) {
			log_err ("Out of memory, %lu", BLOCKS_COLUMNS);
			exit (2);
		}
	}

	pthread_mutex_init (&pgame->lock, NULL);

	return 0;
}

int
blocks_move (struct block_game *pgame, enum block_cmd cmd)
{
	if (pgame == NULL)
		return -1;

	if (pgame->cur == NULL)
		return 0;

	pthread_mutex_lock (&pgame->lock);
	unwrite_cur_piece (pgame);

	switch (cmd) {
	case MOVE_LEFT:
	case MOVE_RIGHT:
		translate_block (pgame, cmd);
		break;
	case MOVE_DROP:
		drop_block (pgame);
		break;
	case ROT_LEFT:
	case ROT_RIGHT:
		rotate_block (pgame, cmd);
		break;
	case SAVE_PIECE:
		if (pgame->save == NULL) {
			pgame->save = pgame->next;
			create_block (&pgame->next);
		} else {
			struct block *tmp;
			tmp = pgame->save;
			pgame->save = pgame->next;
			pgame->next = tmp;
		}
		break;
	}

	write_cur_piece (pgame);
	pthread_mutex_unlock (&pgame->lock);

	return 1;
}

int
blocks_cleanup (struct block_game *pgame)
{
	if (pgame == NULL)
		return -1;

	log_info ("Cleaning game data");
	if (pthread_mutex_trylock (&pgame->lock) != 0)
		debug ("Mutex was locked, unlocking ..");

	pthread_mutex_unlock (&pgame->lock);
	pthread_mutex_destroy (&pgame->lock);

	for (int i = 0; i < BLOCKS_ROWS; i++) {
		free (pgame->spaces[i]);
		free (pgame->colors[i]);
	}

	destroy_block (&pgame->cur);
	destroy_block (&pgame->next);
	destroy_block (&pgame->save);

	return 1;
}
