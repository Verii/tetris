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

#include <ncurses.h>
#include <ncursesw/ncurses.h>

#include "tetris.h"

#define PIECES_CHAR WACS_BLOCK

#define COLORS_LENGTH	7
extern const char *colors;

#define PIECES_Y_OFF 4
#define PIECES_X_OFF 3

#define BOARD_Y_OFF 1
#define BOARD_X_OFF 18

#define BOARD_HEIGHT TETRIS_MAX_ROWS
#define BOARD_WIDTH TETRIS_MAX_COLUMNS +2

#define TEXT_Y_OFF 1
#define TEXT_X_OFF BOARD_X_OFF + BOARD_WIDTH

int screen_init(void);
void screen_cleanup(void);

/* Update screen */
void screen_draw_game(tetris *);

/* Game over! prints high scores if the player lost */
void screen_draw_over(tetris *);

#endif	/* SCREEN_H_ */
