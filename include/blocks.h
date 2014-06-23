#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>

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

struct block {
	enum block_type type;

	/* offsets */
	unsigned char col_off, row_off;
	unsigned char loc[16];
};

/* Meta information about the game */
struct block_game {
	pthread_t	tick;
	pthread_mutex_t	lock;
	unsigned char mod;		/* difficulty */
	unsigned char level;
	int score;
	bool game_over;
	bool *spaces[BLOCKS_ROWS];
	struct block *cur, *next;
};

enum block_cmd { MOVE_LEFT, MOVE_RIGHT, MOVE_DROP, ROT_LEFT, ROT_RIGHT };

/* Game timeline */
int init_blocks (struct block_game *);
int move_blocks (struct block_game *, enum block_cmd);
int destroy_blocks (struct block_game *);

#endif /* BLOCKS_H_ */
