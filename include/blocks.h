#ifndef BLOCKS_H_
#define BLOCKS_H_

#include <pthread.h>
#include <stdbool.h>

#define BLOCKS_ROWS 20 
#define BLOCKS_COLUMNS 10
#define LEN(x) ((sizeof(x)) / (sizeof(*x)))

/* TODO hide this in blocks.c */
struct block {
	/* (x, y): {1, 2, 3, 4} */
	struct loc {
		int x, y;
	} pieces[4];

	/* offsets */
	int col_off, row_off;

	enum block_type {
		SQUARE,
		L,
		J,
		T,
		Z,
		Z_REV,
		LINE,
	} type;
};

/* Meta information about the game */
struct blocks_game {
	pthread_t	tick;
	pthread_mutex_t lock;
	int mod;		/* difficulty */
	int level;
	int score;
	int time;
	bool *spaces[BLOCKS_ROWS];
	struct block *cur, *next;
};

enum block_dir { MOVE_LEFT, MOVE_RIGHT, MOVE_DROP };
enum block_rot { ROT_LEFT, ROT_RIGHT };

/* Game timeline */
int init_blocks (struct blocks_game *);
int move_blocks (struct blocks_game *, enum block_dir, enum block_rot);
int destroy_blocks (struct blocks_game *);

#endif /* BLOCKS_H_ */
