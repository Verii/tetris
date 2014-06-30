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

static void unwrite_cur_piece (struct block_game *);
static void write_cur_piece (struct block_game *);

static void
destroy_block (struct block **dest)
{
	free (*dest);
	*dest = NULL;
}

static void
create_block (struct block **new_block)
{
	struct block *pnb = calloc (1, sizeof *pnb);
	if (pnb == NULL) {
		log_err ("%s", "Out of memory");
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

	/* The position at (0, 0) is the pivot when we rotate */
	if (pnb->type == SQUARE_BLOCK) {
		/* Except square */
		pnb->col_off -= 1; // Help center the piece
		pnb->p[0].x = 0; pnb->p[0].y = -1;
		pnb->p[1].x = 1; pnb->p[1].y = -1;
		pnb->p[2].x = 0; pnb->p[2].y = 0;
		pnb->p[3].x = 1; pnb->p[3].y = 0;
	} else if (pnb->type == LINE_BLOCK) {
		pnb->col_off -= 2;
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

	/* User never gets an invalid block. If we get here, everything was
	 * initialized successfully
	 */
	*new_block = pnb;
	return;
error:
	log_err ("Block type (%d) does not exist", pnb->type);
	destroy_block (new_block);
}

static int
rotate_block (struct block_game *pgame, enum block_cmd cmd)
{
	int py[4], px[4], mod = 1;

	if (pgame->cur->type == SQUARE_BLOCK)
		return 1;

	if (cmd == ROT_LEFT)
		mod = -1;

	for (int i = 0; i < 4; i++) {
		px[i] = pgame->cur->p[i].y * -1 * mod;
		py[i] = pgame->cur->p[i].x * mod;

		int bounds_x, bounds_y;
		bounds_y = py[i] + pgame->cur->row_off;
		bounds_x = px[i] + pgame->cur->col_off;

		if (bounds_x < 0 || bounds_x >= BLOCKS_COLUMNS ||
		    bounds_y < 0 || bounds_y >= BLOCKS_ROWS ||
		    pgame->spaces[bounds_y][bounds_x]) {
			return -1;
		}
	}

	for (int i = 0; i < 4; i++) {
		pgame->cur->p[i].x = px[i];
		pgame->cur->p[i].y = py[i];
	}

	return 1;
}

static int
translate_block (struct block_game *pgame, enum block_cmd cmd)
{
	int dir = 1;

	if (cmd == MOVE_LEFT)
		dir = -1;

	struct point {
		int x, y;
	} check[4];

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		check[i].x = pgame->cur->p[i].x + pgame->cur->col_off + dir;
		check[i].y = pgame->cur->p[i].y + pgame->cur->row_off;
	}

	int hit = 0;
	for (size_t i = 0; i < LEN(check); i++) {
		int y, x;
		y = check[i].y;
		x = check[i].x;

		if (x >= BLOCKS_COLUMNS || x < 0 || pgame->spaces[y][x])
			hit = 1;
	}

	if (hit == 0)
		pgame->cur->col_off += dir;

	return !hit;
}

static int
drop_block (struct block_game *pgame, enum block_cmd cmd)
{
	(void) pgame;
	(void) cmd;
	return -1;
}

static int
destroy_lines (struct block_game *pgame)
{
	uint8_t destroyed = 0;

	/* Check each row for a full line of blocks */
	for (int j, i = BLOCKS_ROWS-1; i >= 2; i--) {
		for (j = 0; j < BLOCKS_COLUMNS; j++) {
			if (pgame->spaces[i][j] == false)
				break;
		}
		if (j < BLOCKS_COLUMNS)
			continue;

		destroyed++;

		free (pgame->spaces[i]);
		log_info ("Removed line %2d", i);

		/* Move everything down */
		for (int k = i; k > 0; k--)
			pgame->spaces[k] = pgame->spaces[k-1];

		/* Add new empty row to top of game */
		pgame->spaces[0] = calloc (BLOCKS_COLUMNS, sizeof (bool));
		if (pgame->spaces[0] == NULL) {
			log_err ("%s", "Out of memory");
			exit (2);
		}

		i++; // lines moved, recheck this one
	}

	pgame->lines_destroyed += destroyed;
	if (pgame->lines_destroyed > pgame->level) {
		pgame->lines_destroyed = 0;
		pgame->level++;

		/* See tests/level-curve.c */
		double speed;
		speed = atan (pgame->level/(double)10) * pgame->mod * 2/PI +1;
		pgame->nsec = (int) ((double)1E9/speed);
	}

	for (int i = destroyed; i > 0; i--)
		pgame->score += pgame->level * pgame->mod;

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
	}
}

static void
write_cur_piece (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return;

	struct loc {
		int x, y;
	} locs[4];

	for (int i = 0; i < 4; i++) {
		int y, x;
		y = pgame->cur->row_off + pgame->cur->p[i].y;
		x = pgame->cur->col_off + pgame->cur->p[i].x; 

		if (y >= BLOCKS_ROWS || x < 0 || x >= BLOCKS_COLUMNS)
			return;

		locs[i].y = y;
		locs[i].x = x;
	}

	for (size_t i = 0; i < LEN(locs); i++)
		pgame->spaces[locs[i].y][locs[i].x] = 1;
}

static int
block_fall (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return 0;

	struct point {
		int x, y;
	} check[4];

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		check[i].x = pgame->cur->p[i].x + pgame->cur->col_off;
		check[i].y = pgame->cur->p[i].y + pgame->cur->row_off +1;
	}

	/* Much easier to just erase the piece */
	unwrite_cur_piece (pgame);

	/* check */
	int hit = 0;
	for (size_t i = 0; i < LEN(check); i++) {
		int y, x;
		y = check[i].y;
		x = check[i].x;

		if (y >= BLOCKS_ROWS || pgame->spaces[y][x])
			hit = 1;
	}

	if (hit) {
		write_cur_piece (pgame);

		bool has_fallen = pgame->cur->fallen;
		destroy_block (&pgame->cur);
		if (has_fallen == false)
			return -1;
	} else {
		pgame->cur->fallen = true;
		pgame->cur->row_off++;

		write_cur_piece (pgame);
	}

	return hit;
}

/*
 * Game is over when this function returns.
 */
int
loop_blocks (struct block_game *pgame)
{
	struct timespec ts;

	if (pgame == NULL)
		return -1;

	ts.tv_sec = 0;
	ts.tv_nsec = pgame->nsec;

	for (;;) {
		nanosleep (&ts, NULL);
		pthread_mutex_lock (&pgame->lock);

		if (block_fall (pgame) < 0) {
			log_info ("%s", "Game over");
			break;
		}

		if (pgame->cur == NULL) {
			/* block hit the bottom */
			pgame->cur = pgame->next;
			create_block (&pgame->next);

			destroy_lines (pgame);
			ts.tv_nsec = pgame->nsec;
		}

		pthread_mutex_unlock (&pgame->lock);
		screen_draw (pgame);
	}

	return 0;
}

int
init_blocks (struct block_game *pgame)
{
	log_info ("%s", "Initializing game data");

	if (pgame == NULL)
		return -1;

	srand (time (NULL));

	pgame->mod = DIFF_NORMAL;
	pgame->game_over = false;
	pgame->level = pgame->score = 0;
	pgame->nsec = 1E9 -1;
	pgame->lines_destroyed = 0;

	create_block (&pgame->cur);
	create_block (&pgame->next);

	for (int i = 0; i < BLOCKS_ROWS; i++) {
		pgame->spaces[i] = calloc (BLOCKS_COLUMNS, sizeof (bool));
		if (!pgame->spaces[i]) {
			log_err ("%s", "Out of memory");
			exit (2);
		}
	}

	pthread_mutex_init (&pgame->lock, NULL);

	return 0;
}

int
move_blocks (struct block_game *pgame, enum block_cmd cmd)
{
	debug ("%s", "Moving blocks");

	if (pgame == NULL)
		return -1;

	pthread_mutex_lock (&pgame->lock);
	unwrite_cur_piece (pgame);

	switch (cmd) {
	case MOVE_LEFT:
	case MOVE_RIGHT:
		translate_block (pgame, cmd);
		break;
	case MOVE_DROP:
		drop_block (pgame, cmd);
		break;
	case ROT_LEFT:
	case ROT_RIGHT:
		rotate_block (pgame, cmd);
		break;
	default:
		break;
	}

	write_cur_piece (pgame);
	pthread_mutex_unlock (&pgame->lock);

	screen_draw (pgame);
	return 1;
}

int
cleanup_blocks (struct block_game *pgame)
{
	log_info ("%s", "Cleaning game data");

	if (pthread_mutex_trylock (&pgame->lock) != 0) {
		debug ("%s", "Mutex was locked, unlocking ..");
	}

	pthread_mutex_unlock (&pgame->lock);
	pthread_mutex_destroy (&pgame->lock);

	for (int i = 0; i < BLOCKS_ROWS; i++)
		free (pgame->spaces[i]);

	destroy_block (&pgame->cur);
	destroy_block (&pgame->next);

	return 0;
}
