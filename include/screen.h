#ifndef SCREEN_H_
#define SCREEN_H_

#include "blocks.h"
#include "db.h"

void screen_init(void);
void screen_cleanup(void);

/* Get user id, filename, etc */
void screen_draw_menu(struct block_game *, struct db_info *);

/* Update screen */
void screen_draw_game(struct block_game *);

/* Game over! prints high scores if the player lost */
void screen_draw_over(struct block_game *, struct db_info *);

#endif				/* SCREEN_H_ */
