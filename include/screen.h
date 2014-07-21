#ifndef SCREEN_H_
#define SCREEN_H_

#include <unistd.h>

#include "blocks.h"
#include "db.h"

#if 0
enum entry_type {
	NULL_ENTRY,
	INT_ENTRY,
	MENU_ENTRY,
	STRING_ENTRY,
};

struct menu_entry {
	char *name;
	enum entry_type type;
	void (*callback)(int, void *);
} entry_items[] = {
	{ "New Game", MENU_ENTRY, NULL },
	{ "Resume", MENU_ENTRY, NULL },
	{ "Settings", MENU_ENTRY, NULL },
	{ "Quit Game", 3, NULL_ENTRY, exit },
};
#endif

void screen_init (void);

/* Get user id, filename, etc */
void screen_draw_menu (struct block_game *, struct db_info *);

/* Update screen */
void screen_draw_game (struct block_game *);

/* Game over! prints high scores if the player lost */
void screen_draw_over (struct block_game *, struct db_info *);

void screen_cleanup (void);

/* User interface thread, accepts input from user and modifies blocks/
 * redraws the screen
 */
void *screen_main (void *);

#endif /* SCREEN_H_ */
