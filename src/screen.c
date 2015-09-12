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
#include "screen/nc.h"
#include "screen/no.h"

void screen_setup(enum SCREEN_MODE mode)
{
	if (mode == SCREEN_MODE_NO) {
		screen_init = screen_nodraw_init;;
		screen_cleanup = screen_nodraw_cleanup;
		screen_menu = screen_nodraw_menu;
		screen_update = screen_nodraw_update;;
		screen_gameover = screen_nodraw_gameover;
	} else if (mode == SCREEN_MODE_NC) {
		screen_init = screen_nc_init;
		screen_cleanup = screen_nc_cleanup;
		screen_menu = screen_nc_menu;
		screen_update = screen_nc_update;
		screen_gameover = screen_nc_gameover;
	}
}
