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

#include "screen.h"
#include "tetris.h"

/* Debugging, no drawing */
int screen_nodraw_init(void) { return 1; }
void screen_nodraw_cleanup(void) { }
int screen_nodraw_menu(tetris *pgame) { (void) pgame; return 1; }
int screen_nodraw_update(tetris *pgame) { (void) pgame; return 1; }
int screen_nodraw_gameover(tetris *pgame) { (void) pgame; return 1; }
