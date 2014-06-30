#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define PI 3.141592653589L

#define BLOCKS_ROWS 22	/* 2 hidden rows above game, where blocks spawn*/
#define BLOCKS_COLUMNS 10
#define LEN(x) ((sizeof(x)) / (sizeof(*x)))

enum block_type {
	SQUARE_BLOCK,
	LINE_BLOCK,
	T_BLOCK,
	L_BLOCK,
	L_REV_BLOCK,
	Z_BLOCK,
	Z_REV_BLOCK,
};

/* Game difficulty */
enum block_diff {
	DIFF_EASY = 1,
	DIFF_NORMAL,
	DIFF_HARD,
};

enum block_cmd {
	MOVE_LEFT,
	MOVE_RIGHT,
	MOVE_DROP,
	ROT_LEFT,
	ROT_RIGHT
};

struct block {
	enum block_type type;
	bool fallen; /* has the block fallen atleast 1 block? */

	/* offsets */
	uint8_t col_off, row_off;
	struct {
		int x, y;
	} p[4];
};

struct block_game {
	pthread_mutex_t	lock;
	uint32_t score;
	uint32_t nsec; /* game tick delay in milliseconds */
	uint16_t lines_destroyed; /* temp. don't print to screen */
	uint8_t level;
	bool *spaces[BLOCKS_ROWS];
	enum block_diff mod;
	struct block *cur, *next;
};

/* Create game state */
int init_blocks (struct block_game *);

/* Send commands to game */
int move_blocks (struct block_game *, enum block_cmd);

/* Main loop, doesn't return until game is over */
int loop_blocks (struct block_game *);

/* Free memory */
int cleanup_blocks (struct block_game *);

#endif /* BLOCKS_H_ */
