#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define PI 3.141592653589L
#define LEN(x) ((sizeof(x))/(sizeof(*x)))

/* These are the size dimensions for the game:
 * Small (8x16)
 * Medium (10x20)
 * Large (12x24)
 */
#define BLOCKS_ROWS 26			/* 2 hidden rows above game */
#define BLOCKS_COLUMNS 12

/* Allow other files to access game */
extern struct block_game *pgame;

struct block_game;
struct block;

struct block_game {
	/* These variables are written to the database
	 * when restoring/saving the game state
	 */
	char id[16];
	uint8_t diff;
	uint8_t width, height, level;
	uint16_t lines_destroyed;	/* temp. don't print to screen */
	uint16_t spaces[BLOCKS_ROWS];	/* array of shorts, one per row.*/
	uint32_t score;

	uint8_t *colors[BLOCKS_ROWS];	/* 1-to-1 with board */
	uint32_t nsec;			/* game tick delay in milliseconds */
	bool loss, pause, quit;		/* how/when we quit */
	pthread_mutex_t	lock;
	struct block *cur, *next, *save;
};

/* Only the currently falling block, the next block, and the save block are
 * stored in this structure. Once a block hits another piece, we forget about
 * it; It becomes part of the game board.
 */
struct block {
	uint8_t col_off, row_off;	/* column/row offsets */
	uint8_t type, color;
	struct {			/* each piece stores a value between */
		int8_t x, y;		/* -1 and +3, offsets are used to */
	} p[4];				/* store the actual location */
};

/* Does a block exist at the specified (y, x) coordinate? */
#define blocks_at_yx(p, y, x) (p->spaces[y] & (1 << x))

void draw_highscores (void);

/* Main loop of the game, self-contained, it will create the game, listen for
 * commands, draw the board, and cleanup when the game is over.
 */
int blocks_main (void);

#endif /* BLOCKS_H_ */
