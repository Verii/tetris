#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>

#include "blocks.h"
#include "debug.h"

#define PI 3.141592653589

static void
_create_block (struct block **new_block)
{
	debug ("%s", "Creating a new random block");

	/* XXX */
	*new_block = malloc (sizeof **new_block);
}

/* TODO testing */
static void
_destroy_lines (struct blocks_game *pgame, int *destroyed)
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

static int
_block_fall (struct blocks_game *pgame)
{
	if (pgame->cur == NULL)
		return 0;

	/* XXX */

	debug ("%s", "Block hit block bottom");
	free (pgame->cur);
	pgame->cur = NULL;

	return 1;
}

/* Game is over when this thread returns.
 * The user may kill the thread, however.
 */
static void *
_game_tick (void *vp)
{
	int hit = 0;
	int lines_destroyed = 0;
	double speed;

	struct timespec ts;
	struct blocks_game *pgame = vp;

	ts.tv_sec = 0;
	ts.tv_nsec = 1000000000 -1;

	for (;;) {
		nanosleep (&ts, NULL);

		/* tick away $hit times */
		if (hit-- > 0)
			continue;

		pthread_mutex_lock (&pgame->lock);

		hit = _block_fall (pgame);
		if (pgame->cur == NULL) {

			pgame->cur = pgame->next; /* Notify next of kin */
			_destroy_lines (pgame, &lines_destroyed);
			_create_block (&pgame->next);

			if (lines_destroyed > pgame->level) {
				pgame->level++;
				lines_destroyed = 0;

				/* See tests/level-curve.c */
				speed = atan (pgame->level/(double)10) *pgame->mod *2/PI +1;
				ts.tv_nsec = (int) ((double)1000000000 / speed);

			}
		}

		pthread_mutex_unlock (&pgame->lock);
	}

	return NULL;
}

int
init_blocks (struct blocks_game *pgame)
{
	log_info ("%s", "Initializing game data");

	pgame->mod = 2;
	pgame->level = pgame->score = pgame->time = 0;

	pgame->next = malloc (sizeof *pgame->next);
	pgame->cur = malloc (sizeof *pgame->cur);

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
	pthread_create (&pgame->tick, NULL, _game_tick, pgame);

	return 0;
}

int
move_blocks (struct blocks_game *pgame, enum block_dir dir, enum block_rot rot)
{
	debug ("%s", "Moving blocks");
	/* XXX */

	(void) pgame;
	(void) dir;
	(void) rot;

	return -1;
}

int
destroy_blocks (struct blocks_game *pgame)
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
