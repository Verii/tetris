#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define PI 3.141592653589L

#define BLOCKS_ROWS 18			/* 2 hidden rows above game */
#define BLOCKS_COLUMNS 8
#define LEN(x) ((sizeof(x))/(sizeof(*x)))

/* Only the currently falling block, the next block, and the save block are
 * stored in this structure. Once a block hits another piece, we forget about
 * it. It becomes part of the game board.
 */
struct block {
	uint8_t col_off, row_off;	/* column/row offsets */
	uint8_t color;
	struct {			/* each piece stores a value between */
		int x, y;		/* -1 and +3, offsets are used to */
	} p[4];				/* store the actual location */
};

/* Game difficulty */
enum block_diff {
	DIFF_EASY,
	DIFF_NORMAL,
	DIFF_HARD,
};

struct block_game {
	/* These variables are written to the database
	 * when restoring/saving the game state
	 */
	uint32_t score;
	uint8_t level;
	uint16_t lines_destroyed;	/* temp. don't print to screen */
	uint8_t spaces[BLOCKS_ROWS];	/* board */
	enum block_diff mod;

	uint8_t *colors[BLOCKS_ROWS];	/* 1-to-1 with board */
	uint32_t nsec;			/* game tick delay in milliseconds */
	bool loss, pause, quit;		/* how/when we quit */
	pthread_mutex_t	lock;
	struct block *cur, *next, *save;
};

enum block_cmd {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_DOWN,			/* Move down one block */
	MOVE_DROP,			/* Drop block to bottom of board */
	ROT_LEFT,
	ROT_RIGHT,
	SAVE_PIECE,
};

#define blocks_at_yx(p, y, x) (p->spaces[y] & (1 << x))

/* Create game state */
int blocks_init (struct block_game *);

/* Send commands to game */
int blocks_move (struct block_game *, enum block_cmd);

/* Main loop, doesn't return until game is over */
int blocks_loop (struct block_game *);

/* Free memory */
int blocks_cleanup (struct block_game *);

#endif /* BLOCKS_H_ */
