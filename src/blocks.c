#include <ctype.h>
#include <ncurses.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "blocks.h"
#include "debug.h"

extern WINDOW *board, *control;
extern const char *colors;
#define BLOCK_CHAR "x"

/* pointer to main game state */
struct block_game game, *pgame = &game;

/* three blocks, adjust block pointers to modify */
static struct block blocks[3];

/* Game difficulty */
enum block_diff {
	DIFF_EASY,
	DIFF_NORMAL,
	DIFF_HARD,
};

/* Movement commands */
enum block_cmd {
	MOVE_LEFT,		/* translate left */
	MOVE_RIGHT,		/* translate right */
	MOVE_COUNTER_CLOCKWISE,	/* rotate counter clockwise */
	MOVE_CLOCKWISE,		/* rotate clockwise */

	/* We don't actually use these .. */
	MOVE_DOWN,		/* Move piece one space down */
	MOVE_DROP,		/* Drop to the bottom of the board */
	MOVE_SAVE_PIECE,	/* save block for later */
};

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
static void
random_block (struct block *block)
{
	if (!block)
		return;

	block->type = rand() %NUM_BLOCKS;

	/* Try to center the block on the board */
	block->col_off = pgame->width/2;
	block->row_off = 1;
	block->color++;

	/* The piece at (0, 0) is the pivot when we rotate */
	switch (block->type) {
	case SQUARE_BLOCK:
		block->p[0].x = -1;	block->p[0].y = -1;
		block->p[1].x =  0;	block->p[1].y = -1;
		block->p[2].x = -1;	block->p[2].y =  0;
		block->p[3].x =  0;	block->p[3].y =  0;
		break;
	case LINE_BLOCK:
		block->col_off--; // center
		block->p[0].x = -1;	block->p[0].y =  0;
		block->p[1].x =  0;	block->p[1].y =  0;
		block->p[2].x =  1;	block->p[2].y =  0;
		block->p[3].x =  2;	block->p[3].y =  0;
		break;
	case T_BLOCK:
		block->p[0].x =  0;	block->p[0].y = -1;
		block->p[1].x = -1;	block->p[1].y =  0;
		block->p[2].x =  0;	block->p[2].y =  0;
		block->p[3].x =  1;	block->p[3].y =  0;
		break;
	case L_BLOCK:
		block->p[0].x =  1;	block->p[0].y = -1;
		block->p[1].x = -1;	block->p[1].y =  0;
		block->p[2].x =  0;	block->p[2].y =  0;
		block->p[3].x =  1;	block->p[3].y =  0;
		break;
	case L_REV_BLOCK:
		block->p[0].x = -1;	block->p[0].y = -1;
		block->p[1].x = -1;	block->p[1].y =  0;
		block->p[2].x =  0;	block->p[2].y =  0;
		block->p[3].x =  1;	block->p[3].y =  0;
		break;
	case Z_BLOCK:
		block->p[0].x = -1;	block->p[0].y = -1;
		block->p[1].x =  0;	block->p[1].y = -1;
		block->p[2].x =  0;	block->p[2].y =  0;
		block->p[3].x =  1;	block->p[3].y =  0;
		break;
	case Z_REV_BLOCK:
		block->p[0].x =  0;	block->p[0].y = -1;
		block->p[1].x =  1;	block->p[1].y = -1;
		block->p[2].x = -1;	block->p[2].y =  0;
		block->p[3].x =  0;	block->p[3].y =  0;
		break;
	}
}

/* Current block hit the bottom of the game; remove it.
 * Swap the "next block" with "current block", and randomize next block.
 */
static inline void
update_cur_next ()
{
	struct block *old = pgame->cur;

	pgame->cur = pgame->next;
	pgame->next = old;

	random_block (pgame->next);
}

/* removes the block from the board */
static void
unwrite_block (struct block *block)
{
	if (!block)
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
static void
write_block (struct block *block)
{
	if (!block)
		return;

	int8_t px[4], py[4];

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

/*
 * rotate pieces in blocks by either 90^ or -90^ around (0, 0) pivot
 */
static int
rotate_block (struct block *block, enum block_cmd cmd)
{
	if (!block)
		return -1;

	if (block->type == SQUARE_BLOCK)
		return 1;

	int8_t mod = 1;
	if (cmd == MOVE_COUNTER_CLOCKWISE)
		mod = -1;

	/* Check each piece for a collision before we write any changes */
	for (size_t i = 0; i < LEN(block->p); i++) {

		int8_t bounds_x, bounds_y;
		bounds_x = block->p[i].y * (-mod) + block->col_off;
		bounds_y = block->p[i].x * ( mod) + block->row_off;

		/* Check out of bounds on each block */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return -1;
	}

	unwrite_block (block);

	/* No collisions, so update the block position. */
	for (size_t i = 0; i < LEN(block->p); i++) {

		int8_t new_x, new_y;
		new_x = block->p[i].y * (-mod);
		new_y = block->p[i].x * mod;

		block->p[i].x = new_x;
		block->p[i].y = new_y;
	}

	write_block (block);

	return 1;
}

/*
 * translate pieces in block horizontally.
 */
static int
translate_block (struct block *block, enum block_cmd cmd)
{
	if (!block)
		return -1;

	int8_t dir = 1;
	if (cmd == MOVE_LEFT)
		dir = -1;

	/* Check each piece for a collision */
	for (size_t i = 0; i < LEN(block->p); i++) {

		int8_t bounds_x, bounds_y;
		bounds_x = block->p[i].x + block->col_off + dir;
		bounds_y = block->p[i].y + block->row_off;

		/* Check out of bounds before we write it */
		if (bounds_x < 0 || bounds_x >= pgame->width ||
		    bounds_y < 0 || bounds_y >= pgame->height ||
		    blocks_at_yx(pgame, bounds_y, bounds_x))
			return -1;
	}

	unwrite_block (block);

	block->col_off += dir;

	write_block (block);

	return 1;
}

/* tries to advance the block one space down */
static int
drop_block (struct block *block)
{
	if (!block)
		return -1;

	for (uint8_t i = 0; i < LEN(block->p); i++) {

		uint8_t bounds_y, bounds_x;
		bounds_y = block->p[i].y + block->row_off +1;
		bounds_x = block->p[i].x + block->col_off;

		if (bounds_y >= pgame->height ||
		    blocks_at_yx (pgame, bounds_y, bounds_x))
			return 0;
	}

	unwrite_block (block);

	block->row_off++;

	write_block (block);

	return 1;
}

static void
update_tick_speed ()
{
	/* See tests/level-curve.c */
	double speed;
	speed = atan (pgame->level/(double)5) * (pgame->diff+1) +1;
	pgame->nsec = (int) ((double)1E9/speed);
}

static void
destroy_lines ()
{
	uint8_t destroyed = 0;

	/* If at any time the first two rows contain a block piece we lose. */
	for (int8_t i = 0; i < 2; i++) {
		for (int8_t j = 0; j < pgame->width; j++) {
			if (blocks_at_yx(pgame, i, j)) {
				pgame->loss = true;
			}
		}
	}

	/* Still continue, even if we've lost to tally points */

	/* Check each row for a full line of blocks */
	for (int8_t i = pgame->height-1; i >= 2; i--) {

		int8_t j = 0;
		for (; j < pgame->width; j++)
			if (blocks_at_yx(pgame, i, j) == 0)
				break;

		if (j != pgame->width)
			continue;

		debug ("Removed line %2d", i+1);
		/* bit field: setting to 0 removes line */
		pgame->spaces[i] = 0;

		destroyed++;

		for (int8_t k = i; k > 0; k--)
			pgame->spaces[k] = pgame->spaces[k-1];

		i++; // recheck this one
	}

	pgame->lines_destroyed += destroyed;

	/* We level up when we destroy (level*2 +5) lines. */
	if (pgame->lines_destroyed >= (pgame->level *2 + 5)) {

		/* level up, reset destroy count */
		pgame->level++;
		pgame->lines_destroyed = 0;

		update_tick_speed ();
	}

	/* If you destroy more than one line at a time then you can get a
	 * points boost. We add points *after* leveling up.
	 */
	pgame->score += destroyed * (pgame->level * (pgame->diff+1));
}

static void
draw_game (void)
{
	/* draw control screen */
	wattrset (control, COLOR_PAIR(3) | A_BOLD);
	mvwprintw (control, 1, 1, "%s", pgame->id);

	wattrset (control, COLOR_PAIR(1));
	mvwprintw (control, 0, 0, "Tetris-" VERSION);
	mvwprintw (control, 2, 1, "Level %d", pgame->level);
	mvwprintw (control, 3, 1, "Score %d", pgame->score);
	mvwprintw (control, 4, 1, "Difficulty %d", pgame->diff);

	mvwprintw (control, 6, 1, "Next   Save");
	mvwprintw (control, 7, 2, "           ");
	mvwprintw (control, 8, 2, "           ");

	mvwprintw (control, 10, 1, "Controls");
	mvwprintw (control, 12, 2, "Pause [F1]");
	mvwprintw (control, 13, 2, "Quit [F3]");
	mvwprintw (control, 14, 2, "Move [asd]");
	mvwprintw (control, 15, 2, "Rotate [qe]");
	mvwprintw (control, 16, 2, "Save [space]");

	for (size_t i = 0; i < LEN(pgame->next->p); i++) {
		char y, x;
		y = pgame->next->p[i].y;
		x = pgame->next->p[i].x;

		wattrset (control, COLOR_PAIR((pgame->next->color
				%LEN(colors)) +1) | A_BOLD);
		mvwprintw (control, y+8, x+3, BLOCK_CHAR);

	}

	for (size_t i = 0; pgame->save && i < LEN(pgame->save->p); i++) {
		char y, x;
		y = pgame->save->p[i].y;
		x = pgame->save->p[i].x;

		wattrset (control, COLOR_PAIR((pgame->save->color
				%LEN(colors)) +1) | A_BOLD);
		mvwprintw (control, y+8, x+9, BLOCK_CHAR);
	}

	wrefresh (control);

	/* game board */
	wattrset (board, A_BOLD | COLOR_PAIR(5));

	mvwvline (board, 0, 0, '*', pgame->height-1);
	mvwvline (board, 0, pgame->width+1, '*', pgame->height-1);
	mvwhline (board, pgame->height-2, 0, '*', pgame->width+2);

	/* Draw the game board, minus the two hidden rows above the game */
	for (int i = 2; i < pgame->height; i++) {
		wmove (board, i-2, 1);

		for (int j = 0; j < pgame->width; j++) {
			if (blocks_at_yx (pgame, i, j)) {
				wattrset (board, COLOR_PAIR((pgame->colors[i][j]
						%LEN(colors)) +1) | A_BOLD);
				wprintw (board, BLOCK_CHAR);
			} else {
				wattrset (board, COLOR_PAIR(1));
				wprintw (board, (j%2) ? "." : " ");
			}
		}
	}

	/* Overlay the PAUSED text */
	wattrset (board, COLOR_PAIR(1) | A_BOLD);
	if (pgame->pause) {
		char y_off, x_off;

		/* Center the text horizontally, place the text slightly above
		 * the middle vertically.
		 */
		x_off = (pgame->width  -6)/2 +1;
		y_off = (pgame->height -2)/2 -2;

		mvwprintw (board, y_off, x_off, "PAUSED");
	}

	wrefresh (board);
}

int
blocks_init (void)
{
	log_info ("Initializing game data");
	memset (pgame, 0, sizeof *pgame);

	pthread_mutex_init (&pgame->lock, NULL);

	/* Default dimensions.
	 * We assume the board is the max size (12x24). If the user wants a
	 * smaller board then we only use the smaller pieces
	 */
	pgame->width = BLOCKS_COLUMNS;
	pgame->height = BLOCKS_ROWS;

	pgame->diff = DIFF_NORMAL;

	pgame->level = 1;

	/* nanosleep() fails if nsec is >= 1E9 */
	pgame->nsec = 1E9 -1;

	/* randomize the initial blocks */
	for (size_t i = 0; i < LEN(blocks); i++) {
		random_block (&blocks[i]);
		/* Start with random color, so cur and next don't follow each
		 * other. Then just increment normally. */
		blocks[i].color = rand();
	}

	pgame->cur = &blocks[0];
	pgame->next = &blocks[1];
	pgame->save = NULL;

	/* Allocate the maximum size necessary to accomodate the
	 * largest board size */
	for (uint8_t i = 0; i < BLOCKS_ROWS; i++) {
		pgame->colors[i] = calloc (BLOCKS_COLUMNS,
				sizeof (*pgame->colors[i]));
		if (!pgame->colors[i]) {
			log_err ("Out of memory");
			exit (2);
		}
	}

	return 1;
}

static int
blocks_cleanup (void)
{
	log_info ("Cleaning game data");
	pthread_mutex_destroy (&pgame->lock);

	/* Free all the memory allocated */
	for (uint8_t i = 0; i < BLOCKS_ROWS; i++)
		free (pgame->colors[i]);

	memset (pgame, 0, sizeof *pgame);

	return 1;
}

void *
blocks_input (void *not_used)
{
	(void) not_used;

	int ch;
	while ((ch = getch())) {

		/* prevent modification of the game from blocks_main
		 * in the other thread */
		pthread_mutex_lock (&pgame->lock);

		switch (toupper(ch)) {
		case 'A':
			translate_block (pgame->cur, MOVE_LEFT);
			break;
		case 'D':
			translate_block (pgame->cur, MOVE_RIGHT);
			break;
		case 'S':
			drop_block (pgame->cur);
			break;
		case 'W':
			/* drop the block to the bottom of the game */
			while (drop_block (pgame->cur) == 1);
			break;
		case 'Q':
			rotate_block (pgame->cur, MOVE_COUNTER_CLOCKWISE);
			break;
		case 'E':
			rotate_block (pgame->cur, MOVE_CLOCKWISE);
			break;
		case ' ':
			/* Swap next and save pieces */
			if (pgame->save == NULL) {
				pgame->save = &blocks[2];
			}

			struct block *tmp = pgame->save;
			pgame->save = pgame->next;
			pgame->next = tmp;

			break;
		case KEY_F(1):
			pgame->pause = !pgame->pause;
			break;
		case KEY_F(3):
			pgame->pause = false;
			pgame->quit = true;
			break;
		default:
			break;
		}

		draw_game ();

		pthread_mutex_unlock (&pgame->lock);
	}

	return NULL;
}

int
blocks_main (void)
{
	wclear (control);
	wclear (board);
	draw_game ();

	pthread_t input;
	pthread_create (&input, NULL, blocks_input, NULL);

	struct timespec ts;
	ts.tv_sec = 0;
	ts.tv_nsec = 0;

	/* The database sets the current level for the game.
	 * Update the tick delay so we resume at proper difficulty
	 */
	update_tick_speed ();

	for (;;) {
		ts.tv_nsec = pgame->nsec;
		nanosleep (&ts, NULL);

		if (pgame->pause)
			continue;

		if (pgame->loss || pgame->quit)
			break;

		pthread_mutex_lock (&pgame->lock);

		if (drop_block (pgame->cur) == 0) {
			destroy_lines ();
			update_cur_next ();
		}

		draw_game ();

		pthread_mutex_unlock (&pgame->lock);
	}

	/* Remove the current piece from the board.
	 * Otherwise it would save the location of a block in mid-air.
	 * We can't restore from blocks like that, so just remove it.
	 */
	unwrite_block (pgame->cur);

	pthread_cancel (input);

	blocks_cleanup ();

	return 1;
}
