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

#include "blocks.h"
#include "db.h"

void screen_init(void);
void screen_cleanup(void);

/* Get user id, filename, etc */
void screen_draw_menu(void);

/* Update screen */
void screen_draw_game(void);

/* Game over! prints high scores if the player lost */
void screen_draw_over(void);

#endif				/* SCREEN_H_ */
