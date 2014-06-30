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

void *screen_main (void *);
void  screen_draw (struct block_game *);
void  screen_cleanup (void);

#endif /* SCREEN_H_ */
