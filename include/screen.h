#ifndef SCREEN_H_
#define SCREEN_H_

#include "blocks.h"
#include "db.h"

void screen_init ();

/* Get user id, filename, etc */
void screen_draw_menu (struct block_game *, struct db_info *);
void screen_draw_game (struct block_game *);
void screen_draw_over (struct block_game *, struct db_info *);

void screen_cleanup ();

/* UI thread */
void *screen_main (void *);

#endif /* SCREEN_H_ */
