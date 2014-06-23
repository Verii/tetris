#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "blocks.h"
#include "debug.h"

#define PI 3.141592653589

static void
create_block (struct block **new_block)
{
	debug ("%s", "Creating a new random block");

	struct block *pnb;
	pnb = calloc (1, sizeof **new_block);
	if (pnb == NULL)
		log_err ("%s", "Out of memory");

	pnb->col_off = BLOCKS_COLUMNS/2;
	pnb->row_off = 0;

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

	int *values = NULL;

	switch (pnb->type) {
	case SQUARE_BLOCK:
		values = (int []) {
			1, 1, 0, 0,
			1, 1, 0, 0,
		       	0, 0, 0, 0,
			0, 0, 0, 0};
		break;
	case LINE_BLOCK:
		values = (int []) {
			1, 0, 0, 0,
			1, 0, 0, 0,
			1, 0, 0, 0,
			1, 0, 0, 0};
		break;
	case T_BLOCK:
		values = (int []) {
			1, 0, 0, 0,
			1, 1, 0, 0,
			1, 0, 0, 0,
			0, 0, 0, 0};
		break;
	case L_BLOCK:
		values = (int []) {
			1, 0, 0, 0,
			1, 0, 0, 0,
			1, 1, 0, 0,
			0, 0, 0, 0};
		break;
	case L_REV_BLOCK:
		values = (int []) {
			0, 1, 0, 0,
			0, 1, 0, 0,
			1, 1, 0, 0,
			0, 0, 0, 0};
		break;
	case Z_BLOCK:
		values = (int []) {
			1, 1, 0, 0,
			0, 1, 1, 0,
			0, 0, 0, 0,
			0, 0, 0, 0};
		break;
	case Z_REV_BLOCK:
		values = (int []) {
			0, 1, 1, 0,
			1, 1, 0, 0,
			0, 0, 0, 0,
			0, 0, 0, 0};
		break;
	default:
		goto error;
	}

	memcpy (pnb->loc, values, sizeof pnb->loc);
	*new_block = pnb;
	return;

error:
	log_err ("Block type (%d) does not exist", pnb->type);
	free (pnb);
}

static void
destroy_lines (struct block_game *pgame, int *destroyed)
{
	int i, j;

	debug ("%s", "Checking for full lines");

	/* Check each row for a full line of blocks */
	for (i = BLOCKS_ROWS-1; i >= 0; i--) {
		for (j = 0; j < BLOCKS_COLUMNS; j++) {
			if (pgame->spaces[i][j] == false)
				break;
		}
		if (j < BLOCKS_COLUMNS)
			continue;

		*destroyed = *destroyed +1;

		/* Remove full line */
		free (pgame->spaces[i]);
		log_info ("Line full! Removed line %2d", i);

		/* Move everything down */
		for (int k = i; k > 0; k--)
			pgame->spaces[k] = pgame->spaces[k-1];

		/* Add new empty row to top of game */
		pgame->spaces[0] = calloc (BLOCKS_COLUMNS, sizeof (bool));

		/* Re-check this row, because we moved everything */
		i++;
	}
}

static void
write_cur_piece (struct block_game *pgame, bool val)
{
	if (pgame->cur == NULL)
		return;

	int count = 0;
	struct loc {
		int x, y;
	} locs[4];

	for (size_t i = 0; i < LEN(pgame->cur->loc); i++) {
		if (pgame->cur->loc[i] == 0)
			continue;

		int y, x;
		y = pgame->cur->row_off + i/4;
		x = pgame->cur->col_off + i%4;

		if (y >= BLOCKS_ROWS || x < 0 || x >= BLOCKS_COLUMNS)
			return;

		locs[count].y = y;
		locs[count].x = x;
		count++;
	}

	if (count != 4)
		return;

	for (size_t i = 0; i < LEN(locs); i++)
		pgame->spaces[locs[i].y][locs[i].x] = val;
}

static int
block_fall (struct block_game *pgame)
{
	if (pgame->cur == NULL)
		return 0;

	int hit = 0;
	int low_y[4] = { 0 }; /* lowest y of each x */

	for (int i = 0; i < 4; i++) {
		for (int j = 0; j < 4; j++) {
			if (pgame->cur->loc[i*4 +j] == 0)
				continue;
			low_y[j] = i+1;
		}
	}

	for (size_t i = 0; i < LEN(low_y); i++) {
		if (low_y[i] == 0)
			continue;

		int check, x;
		check = pgame->cur->row_off + low_y[i];
		x = pgame->cur->col_off + i;
		if (pgame->spaces[check][x] || check >= BLOCKS_ROWS) {
			hit = 1;
			break;
		}
	}

	if (hit) {
		debug ("%s", "Block hit block bottom");
		free (pgame->cur);
		pgame->cur = NULL;
	} else {
		write_cur_piece (pgame, false);
		pgame->cur->row_off++;
		write_cur_piece (pgame, true);
	}

	return hit;
}

/* Game is over when this thread returns.
 * The user may kill the thread, however.
 */
static void *
game_tick (void *vp)
{
	int hit = 0;
	int lines_destroyed = 0;

	struct timespec ts;
	struct block_game *pgame = vp;

	ts.tv_sec = 0;
	ts.tv_nsec = 1000000000 -1;

	for (;;) {
		nanosleep (&ts, NULL);

		/* tick away $hit times */
		if (hit-- > 0)
			continue;

		pthread_mutex_lock (&pgame->lock);

		hit = block_fall (pgame);

		/* Block was unable to fall a single space */
		if (hit < 0)
			pgame->game_over = true;

		if (pgame->cur == NULL) {
			pgame->cur = pgame->next; /* Notify next of kin */
			create_block (&pgame->next);

			destroy_lines (pgame, &lines_destroyed);

			if (lines_destroyed > pgame->level) {
				pgame->level++;
				lines_destroyed = 0;

				/* See tests/level-curve.c */
				double speed;
				speed = atan (pgame->level/(double)10)
					* pgame->mod * 2/PI +1;
				ts.tv_nsec = (int) ((double)1000000000/speed);

			}
		}

		pthread_mutex_unlock (&pgame->lock);
	}

	return NULL;
}

int
init_blocks (struct block_game *pgame)
{
	log_info ("%s", "Initializing game data");

	pgame->mod = 2;
	pgame->game_over = false;
	pgame->level = pgame->score = 0;

	create_block (&pgame->cur);
	create_block (&pgame->next);

	if (!(pgame->next && pgame->cur)) {
		log_err ("%s", "Out of memory");
		exit (2);
	}

	for (int i = 0; i < BLOCKS_ROWS; i++) {
		pgame->spaces[i] = calloc (BLOCKS_COLUMNS, sizeof (bool));
		if (!pgame->spaces[i]) {
			log_err ("%s", "Out of memory");
			exit (2);
		}
	}

	pthread_mutex_init (&pgame->lock, NULL);
	pthread_create (&pgame->tick, NULL, game_tick, pgame);

	return 0;
}

int
move_blocks (struct block_game *pgame, enum block_cmd cmd)
{
	debug ("%s", "Moving blocks");

	/* XXX */

	(void) pgame;
	(void) cmd;

	return -1;
}

int
destroy_blocks (struct block_game *pgame)
{
	log_info ("%s", "Destroying game data");

	/* XXX Make sure this is unlocked */
	pthread_mutex_destroy (&pgame->lock);

	pthread_cancel (pgame->tick);

	for (int i = 0; i < BLOCKS_ROWS; i++)
		free (pgame->spaces[i]);

	free (pgame->cur);
	free (pgame->next);

	return 0;
}
