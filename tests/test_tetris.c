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

#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <time.h>
#include <unistd.h>

#include "tetris.c"

int main(void)
{
	tetris *pgame;

	setlocale(LC_ALL, "");
	srand(time(NULL));

	if (tetris_init(&pgame) != 1)
		log_err("Failed to initialize Tetris");

	tetris_set_attr(pgame, TETRIS_SET_NAME, "Theta");
	tetris_set_attr(pgame, TETRIS_SET_DBFILE, "test-db");

	char name[16];

	tetris_get_attr(pgame, TETRIS_GET_NAME, name, sizeof name);
	printf("Player name: %s\n", name);

	tetris_get_attr(pgame, TETRIS_GET_DBFILE, name, sizeof name);
	printf("Database file: %s\n", name);

	int val;
	tetris_get_attr(pgame, TETRIS_GET_SCORE, &val);
	printf("Score: %d\n", val);

	tetris_cleanup(pgame);

	return 0;
}
