#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>

#define PI 3.141592653589L

#define BLOCKS_ROWS 20 
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

enum block_diff {
	DIFF_EASY = 1,
	DIFF_NORMAL,
	DIFF_HARD,
};

struct block {
	enum block_type type;
	bool fallen; /* has the block fallen atleast 1 block */

	/* offsets */
	uint8_t col_off, row_off;
	bool loc[16];
};

struct block_game {
	pthread_mutex_t	lock;
	uint32_t score;
	uint32_t nsec;
	uint16_t lines_destroyed;
	uint8_t level;
	bool game_over;
	bool *spaces[BLOCKS_ROWS];
	enum block_diff mod;
	struct block *cur, *next;
};

enum block_cmd { MOVE_LEFT, MOVE_RIGHT, MOVE_DROP, ROT_LEFT, ROT_RIGHT };

/* Game timeline */
int init_blocks (struct block_game *);
int move_blocks (struct block_game *, enum block_cmd);
int loop_blocks (struct block_game *);
int cleanup_blocks (struct block_game *);

#endif /* BLOCKS_H_ */
