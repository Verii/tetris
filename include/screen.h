/*
 * Copyright (C) 2014  James Smith <james@apertum.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef SCREEN_H_
#define SCREEN_H_

#include "tetris.h"

/*****************************************************************/
/* Function pointers to call upon different drivers to draw game */
/*****************************************************************/

/* init/deinit */
int (*screen_init)(void);
void (*screen_cleanup)(void);
/* Game menu */
int (*screen_menu)(tetris *);
/* Update screen */
int (*screen_update)(tetris *);
/* Game over! prints high scores if the player lost */
int (*screen_gameover)(tetris *);

/* Debugging, no drawing */
int screen_nodraw_init(void);
void screen_nodraw_cleanup(void);
int screen_nodraw_menu(tetris *);
int screen_nodraw_update(tetris *);
int screen_nodraw_gameover(tetris *);

/* Text */
int screen_text_init(void);
void screen_text_cleanup(void);
int screen_text_menu(tetris *);
int screen_text_update(tetris *);
int screen_text_gameover(tetris *);

/* Ncurses */
int screen_nc_init(void);
void screen_nc_cleanup(void);
int screen_nc_menu(tetris *);
int screen_nc_update(tetris *);
int screen_nc_gameover(tetris *);

/* GL */
int screen_gl_init(void);
void screen_gl_cleanup(void);
int screen_gl_menu(tetris *);
int screen_gl_update(tetris *);
int screen_gl_gameover(tetris *);

#endif	/* SCREEN_H_ */
