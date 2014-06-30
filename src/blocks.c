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

	if (pnb->type == SQUARE_BLOCK)
		pnb->loc[0] = pnb->loc[1] = pnb->loc[4] = pnb->loc[5] = 1;
	else if (pnb->type == LINE_BLOCK)
		pnb->loc[0] = pnb->loc[4] = pnb->loc[8] = pnb->loc[12] = 1;
	else if (pnb->type == T_BLOCK)
		pnb->loc[0] = pnb->loc[4] = pnb->loc[5] = pnb->loc[8] = 1;
	else if (pnb->type == L_BLOCK)
		pnb->loc[0] = pnb->loc[4] = pnb->loc[8] = pnb->loc[9] = 1;
	else if (pnb->type == L_REV_BLOCK)
		pnb->loc[1] = pnb->loc[5] = pnb->loc[8] = pnb->loc[9] = 1;
	else if (pnb->type == Z_BLOCK)
		pnb->loc[0] = pnb->loc[1] = pnb->loc[5] = pnb->loc[6] = 1;
	else if (pnb->type == Z_REV_BLOCK)
		pnb->loc[1] = pnb->loc[2] = pnb->loc[4] = pnb->loc[5] = 1;
	else
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
	(void) pgame;
	(void) cmd;
	return -1;
}

static int
translate_block (struct block_game *pgame, enum block_cmd cmd)
{
	(void) pgame;
	(void) cmd;
	return -1;
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
	int destroyed = 0;

	/* Check each row for a full line of blocks */
	for (int j, i = BLOCKS_ROWS-1; i >= 0; i--) {
		for (j = 0; j < BLOCKS_COLUMNS; j++) {
			if (pgame->spaces[i][j] == false)
				break;
		}
		if (j < BLOCKS_COLUMNS)
			continue;

		destroyed++;

		free (pgame->spaces[i]);
		log_info ("Line full! Removed line %2d", i);

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
	}

	for (int i = destroyed; i >= 0; i--) {
		pgame->score += pgame->level;
	}

	/* See tests/level-curve.c */
	double speed;
	speed = atan (pgame->level/(double)10) * pgame->mod * 2/PI +1;
	pgame->nsec = (int) ((double)1E9/speed);

	return destroyed;
}

static void
unwrite_cur_piece (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return;

	for (size_t i = 0; i < LEN(pgame->cur->loc); i++) {
		if (pgame->cur->loc[i] == false)
			continue;
		int y, x;
		y = pgame->cur->row_off + (i/4);
		x = pgame->cur->col_off + (i%4);

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

	int count = 0;
	for (size_t i = 0; i < LEN(pgame->cur->loc); i++) {
		if (pgame->cur->loc[i] == false)
			continue;

		int y, x;
		y = pgame->cur->row_off + (i/4);
		x = pgame->cur->col_off + (i%4);

		if (y >= BLOCKS_ROWS || x < 0 || x >= BLOCKS_COLUMNS)
			return;

		locs[count].y = y;
		locs[count].x = x;
		count++;
	}

	/* every piece needs to be writable before we continue */
	if (count != 4)
		return;

	/* commit */
	for (size_t i = 0; i < LEN(locs); i++) {
		pgame->spaces[locs[i].y][locs[i].x] = 1;
	}
}

static int
block_fall (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return 0;

	int low_y[4] = { 0 };
	for (size_t i = 0; i < LEN(pgame->cur->loc); i++)
		if (pgame->cur->loc[i])
			low_y[i%4] = (i/4) +1;

	/* Check if there's a block beneath any piece */
	int hit = 0;
	for (size_t i = 0; i < LEN(low_y); i++) {
		if (low_y[i] == 0)
			continue;

		int y, x;
		y = pgame->cur->row_off + low_y[i];
		x = pgame->cur->col_off + i;

		if (pgame->spaces[y][x] || y >= BLOCKS_ROWS) {
			hit = 1;
			break;
		}
	}

	if (hit) {
		/* we hit the bottom */
		bool has_fallen = pgame->cur->fallen;
		destroy_block (&pgame->cur);

		/* Game over condition */
		if (has_fallen == false)
			return -1;
	} else {
		pgame->cur->fallen = true;
		unwrite_cur_piece (pgame);
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

		if (block_fall (pgame) < 0)
			break;

		if (pgame->cur == NULL) {
			/* block hit the bottom */
			destroy_lines (pgame);
			ts.tv_nsec = pgame->nsec;

			pgame->cur = pgame->next;
			create_block (&pgame->next);
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
		return 0;
	}

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
