#ifndef SCREEN_H_
#define SCREEN_H_

#include "blocks.h"

struct block_names {
	enum block_type block;
	char *name;
};

struct level_names {
	enum block_diff difficulty;
	char *name;
};

/* Starter thread, main.c */
void *screen_main (void *);

/* Print game to screen, used in screen.c and blocks.c */
void  screen_draw (struct block_game *);

/* Cleanup, shutdown ncurses, etc. used in main.c */
void  screen_cleanup (struct block_game *);

#endif /* SCREEN_H_ */
