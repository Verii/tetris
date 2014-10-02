#include <ctype.h>
#include <math.h>
#include <ncurses.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"
#include "screen.h"

/* three blocks,
 * current falling block,
 * next block to fall,
 * save block
 */
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
 * randomizes block and sets the initial positions of the pieces
 */
static void create_block(struct block_game *pgame, struct block *block)
{
	if (!pgame || !block)
		return;

	block->type = rand() % NUM_BLOCKS;

	/* Try to center the block on the board */
	block->col_off = pgame->width / 2;
	block->row_off = 1;
	block->color++;

	/* The piece at (0, 0) is the pivot when we rotate */
	switch (block->type) {
	case SQUARE_BLOCK:
		block->p[0].x = -1; block->p[0].y = -1;
		block->p[1].x = 0; block->p[1].y = -1;
		block->p[2].x = -1; block->p[2].y = 0;
		block->p[3].x = 0; block->p[3].y = 0;
		break;
	case LINE_BLOCK:
		block->col_off--;	/* center */
		block->p[0].x = -1; block->p[0].y = 0;
		block->p[1].x = 0; block->p[1].y = 0;
		block->p[2].x = 1; block->p[2].y = 0;
		block->p[3].x = 2; block->p[3].y = 0;
		break;
	case T_BLOCK:
		block->p[0].x = 0; block->p[0].y = -1;
		block->p[1].x = -1; block->p[1].y = 0;
		block->p[2].x = 0; block->p[2].y = 0;
		block->p[3].x = 1; block->p[3].y = 0;
		break;
	case L_BLOCK:
		block->p[0].x = 1; block->p[0].y = -1;
		block->p[1].x = -1; block->p[1].y = 0;
		block->p[2].x = 0; block->p[2].y = 0;
		block->p[3].x = 1; block->p[3].y = 0;
		break;
	case L_REV_BLOCK:
		block->p[0].x = -1; block->p[0].y = -1;
		block->p[1].x = -1; block->p[1].y = 0;
		block->p[2].x = 0; block->p[2].y = 0;
		block->p[3].x = 1; block->p[3].y = 0;
		break;
	case Z_BLOCK:
		block->p[0].x = -1; block->p[0].y = -1;
		block->p[1].x = 0; block->p[1].y = -1;
		block->p[2].x = 0; block->p[2].y = 0;
		block->p[3].x = 1; block->p[3].y = 0;
		break;
	case Z_REV_BLOCK:
		block->p[0].x = 0; block->p[0].y = -1;
		block->p[1].x = 1; block->p[1].y = -1;
		block->p[2].x = -1; block->p[2].y = 0;
		block->p[3].x = 0; block->p[3].y = 0;
		break;
	}
}

/* Current block hit the bottom of the game, remove it.
 * Swap the "next block" with "current block", and randomize next block.
 */
static inline void update_cur_next(struct block_game *pgame)
{
	if (!pgame)
		return;

	struct block *old = pgame->cur;

	pgame->cur = pgame->next;
	pgame->next = old;

	create_block(pgame, pgame->next);
}

/* rotate pieces in blocks by either 90^ or -90^ around (0, 0) pivot */
static int rotate_block(struct block_game *pgame, struct block *block,
			enum block_cmd cmd)
{
	if (!pgame || !block)
		return -1;

	if (block->type == SQUARE_BLOCK)
		return 1;

	int mod = 1;
	if (cmd == ROT_LEFT)
		mod = -1;

	/* Check each piece for a collision before we write any changes */
	for (size_t i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
		bounds_x = block->p[i].y * (-mod) + block->col_off;
		bounds_y = block->p[i].x * (mod) + block->row_off;

		/* Check for out of bounds on each block */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return -1;
	}

	/* No collisions, so update the block position. */
	for (size_t i = 0; i < LEN(block->p); i++) {
		int new_x, new_y;
		new_x = block->p[i].y * (-mod);
		new_y = block->p[i].x * (mod);

		block->p[i].x = new_x;
		block->p[i].y = new_y;
	}

	return 1;
}

/* translate pieces in block horizontally. */
static int translate_block(struct block_game *pgame, struct block *block,
			   enum block_cmd cmd)
{
	if (!pgame || !block)
		return -1;

	int dir = 1;
	if (cmd == MOVE_LEFT)
		dir = -1;

	/* Check each piece for a collision */
	for (size_t i = 0; i < LEN(block->p); i++) {
		int bounds_x, bounds_y;
		bounds_x = block->p[i].x + block->col_off + dir;
		bounds_y = block->p[i].y + block->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return -1;
	}

	block->col_off += dir;

	return 1;
}

static inline void update_tick_speed(struct block_game *pgame)
{
	if (!pgame)
		return;

	/* See tests/level-curve.c */
	double speed = 1.2;
	speed += atan(pgame->level / 5.0L) * 2 / PI * pgame->diff;
	pgame->nsec = (1E9 / speed) - 1;
}

static int destroy_lines(struct block_game *pgame)
{
	if (!pgame)
		return -1;

	uint32_t full_row = ~(UINT32_MAX << pgame->width);
	unsigned int destroyed = 0;

	/* The first two rows are 'above' the game, that's where the new blocks
	 * come into existence. We lose if there's ever a block there. */
	for (size_t i = 0; i < 2; i++)
		if (pgame->spaces[i])
			pgame->loss = true;

	/* Check each row for a full line,
	 * we check in reverse to ease moving the rows down when we find a full
	 * row. Ignore the top two rows, which we checked above.
	 */
	for (size_t i = pgame->height - 1; i >= 2; i--) {
		if (full_row != pgame->spaces[i])
			continue;

		/* bit field: setting to 0 removes line */
		pgame->spaces[i] = 0;

		destroyed++;

		/* Move lines above destroyed line down */
		for (size_t k = i; k > 0; k--)
			pgame->spaces[k] = pgame->spaces[k - 1];

		/* Lines are moved down, so we recheck this row. */
		i++;
	}

	pgame->lines_destroyed += destroyed;
	if (pgame->lines_destroyed >= (pgame->level * 2 + 2)) {
		pgame->lines_destroyed -= (pgame->level * 2 + 2);
		pgame->level++;
		update_tick_speed(pgame);
	}

	/* Destroying >1 rows yields more points if you hit a level boundary */
	pgame->score += destroyed * pgame->level * pgame->diff;

	return destroyed;
}

/* removes the block from the board */
static void unwrite_piece(struct block_game *pgame, struct block *block)
{
	if (!pgame || !block)
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

/* writes the piece to the board, checking bounds and collisions */
static void write_piece(struct block_game *pgame, struct block *block)
{
	if (!pgame || !block)
		return;

	int px[4], py[4];

	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		py[i] = block->row_off + block->p[i].y;
		px[i] = block->col_off + block->p[i].x;

		if (px[i] < 0 || px[i] >= pgame->width ||
		    py[i] < 0 || py[i] >= pgame->height)
			return;
	}

	/* pgame->spaces is an array of bit fields, 1 per row */
	for (size_t i = 0; i < LEN(pgame->cur->p); i++) {
		/* Set the bit where the block exists */
		pgame->spaces[py[i]] |= (1 << px[i]);
		pgame->colors[py[i]][px[i]] = block->color;
	}
}

/* tries to advance the block one space down */
static int drop_block(struct block_game *pgame, struct block *block)
{
	if (!pgame || !block)
		return -1;

	for (unsigned int i = 0; i < 4; i++) {
		unsigned int bounds_y, bounds_x;
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

int blocks_init(struct block_game *pgame)
{
	if (!pgame)
		return -1;

	log_info("Initializing game data");
	memset(pgame, 0, sizeof *pgame);

	pthread_mutex_init(&pgame->lock, NULL);

	/* Default dimensions, modified in screen_draw_menu()
	 * We assume the board is the max size (12x24). If the user wants a
	 * smaller board, we use only the smaller pieces.
	 */
	pgame->width = BLOCKS_COLUMNS;
	pgame->height = BLOCKS_ROWS;

	/* Default difficulty: modified in screen_draw_menu() */
	pgame->diff = DIFF_NORMAL;

	pgame->level = 1;
	pgame->nsec = 1E9 - 1;

	/* randomize the initial blocks */
	for (size_t i = 0; i < LEN(blocks); i++) {
		create_block(pgame, &blocks[i]);
		/* Start with random color, so cur and next don't follow each
		 * other.
		 */
		blocks[i].color = rand();
	}

	pgame->cur = &blocks[0];
	pgame->next = &blocks[1];
	pgame->save = NULL;

	/* Allocate the maximum size necessary to accomodate the
	 * largest board size */
	for (int i = 0; i < BLOCKS_ROWS; i++) {
		pgame->colors[i] = calloc(BLOCKS_COLUMNS,
					  sizeof(*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err("Out of memory");
			exit(EXIT_FAILURE);
		}
	}

	return 1;
}

int blocks_cleanup(struct block_game *pgame)
{
	if (!pgame)
		return -1;

	log_info("Cleaning game data");
	pthread_mutex_destroy(&pgame->lock);

	/* Free all the memory allocated */
	for (int i = 0; i < BLOCKS_ROWS; i++)
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
	struct block_game *pgame = vp;
	if (!pgame)
		return NULL;

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 0;

	/* When we read in from the database, it sets the current level
	 * for the game. Update the tick delay so we resume at proper
	 * difficulty.
	 */
	update_tick_speed(pgame);

	while (1) {
		ts.tv_nsec = pgame->nsec;
		nanosleep(&ts, NULL);

		if (pgame->pause)
			continue;

		if (pgame->loss || pgame->quit)
			break;

		pthread_mutex_lock(&pgame->lock);

		unwrite_piece(pgame, pgame->cur);
		int hit = drop_block(pgame, pgame->cur);
		write_piece(pgame, pgame->cur);

		if (hit == 0) {
			update_cur_next(pgame);
			destroy_lines(pgame);
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
	struct block_game *pgame = vp;
	if (!pgame || !pgame->cur)
		return NULL;

	int ch;
	while ((ch = getch())) {
		/* prevent modification of the game from blocks_loop in the other
		 * thread */
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
			drop_block(pgame, pgame->cur);
			break;
		case 'W':
			/* drop the block to the bottom of the game */
			while (drop_block(pgame, pgame->cur)) ;
			break;
		case 'Q':
			rotate_block(pgame, pgame->cur, ROT_LEFT);
			break;
		case 'E':
			rotate_block(pgame, pgame->cur, ROT_RIGHT);
			break;
		case ' ':{
				if (pgame->save == NULL) {
					pgame->save = &blocks[2];
				}

				struct block *tmp = pgame->save;

				pgame->save = pgame->next;
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
