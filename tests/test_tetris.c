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

#include "tetris.h"
#include "logs.h"

int main(void)
{
	tetris *pgame;

	setlocale(LC_ALL, "");
	srand(time(NULL));

	if (tetris_init(&pgame) != 1)
		log_err("Failed to initialize Tetris");

	tetris_set_wallkicks(pgame, TETRIS_FALSE);
	tetris_set_tspins(pgame, TETRIS_FALSE);
	tetris_set_ghosts(pgame, TETRIS_FALSE);
	tetris_set_name(pgame, "Theta");
	tetris_set_dbfile(pgame, "test-db");

	tetris_cmd(pgame, TETRIS_MOVE_DROP);
	tetris_cmd(pgame, TETRIS_GAME_TICK);

	printf("Level: %d\n", tetris_get_level(pgame));
	printf("Score: %d\n", tetris_get_score(pgame));
	printf("Lines destroyed: %d\n", tetris_get_lines(pgame));
	printf("Nanosecond tick delay: %d\n", tetris_get_delay(pgame));
	printf("Ghost blocks enabled: %d\n", tetris_get_ghosts(pgame));
	printf("Wallkicks enabled: %d\n", tetris_get_wallkicks(pgame));
	printf("T-Spins enabled: %d\n", tetris_get_tspins(pgame));

	char name[16];

	tetris_get_name(pgame, name, sizeof name);
	printf("Player name: \"%s\"\n", name);

	tetris_get_dbfile(pgame, name, sizeof name);
	printf("Database file: \"%s\"\n", name);

	tetris_cleanup(pgame);

	return 0;
}
