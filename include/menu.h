#ifndef MENU_H_
#define MENU_H_

#include <ncurses.h>

void curses_init (void);
void curses_kill (void);

void draw_menu (void);
void draw_settings (void);

#endif /* MENU_H_ */
