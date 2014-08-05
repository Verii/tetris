#ifndef MENU_H_
#define MENU_H_

#include <ncurses.h>

void curses_init (void);
void curses_clean (void);

void draw_menu (void);
void draw_settings (void);
void draw_highscores (void);
void draw_resume (void);

#endif /* MENU_H_ */
