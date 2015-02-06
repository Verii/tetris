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

#include <string.h>

#include <json-c/json.h>
#include "tetris.h"

/*
int net_deserialize(tetris *pgame, const char *str) {
	json_object *obj;
	obj = json_tokener_str(str);
}
*/

int net_serialize(tetris *pgame, char **str)
{
	json_object *obj;

	obj = json_object_new_object();

	char username[16];
	tetris_get_name(pgame, username, sizeof username);
	json_object_object_add(obj, "username",
			json_object_new_string(username));

	int level = tetris_get_level(pgame);;
	json_object_object_add(obj, "level",
			json_object_new_int(level));

	int score = tetris_get_score(pgame);
	json_object_object_add(obj, "score",
			json_object_new_int(score));

	switch (tetris_get_state(pgame)) {
	case TETRIS_LOSE:
		json_object_object_add(obj, "type",
			json_object_new_string("lose"));
		break;
	case TETRIS_WIN:
	case TETRIS_QUIT:
		json_object_object_add(obj, "type",
			json_object_new_string("quit"));
		break;
	case TETRIS_PAUSED:
	default:
		json_object_object_add(obj, "type",
			json_object_new_string("update"));
		break;
	}

#if 0
	/* Get board/colors data from game in pack()'d form */
	char *board = NULL, *colors = NULL;
	tetris_get_attr(pgame, TETRIS_GET_BOARD, &board);
	tetris_get_attr(pgame, TETRIS_GET_COLORS, &colors);

	/* Unpack() array, add to json array object */
	json_object *board_array, *colors_array;
	board_array = json_object_new_array();
	colors_array = json_object_new_array();

	int len = board[0];
	json_object_array_add(board_array,
			json_object_new_int(len));
	for (int i = 1; i < len; i += 2)
		json_object_array_add(board_array,
			json_object_new_int(board[i]));
	free(board);

	len = colors[0];
	for (int i = 0; i < len; i++)
		json_object_array_add(colors_array,
			json_object_new_int(colors[i]));
	free(colors);

	/* Add array objects to json object */
	json_object_object_add(obj, "board", board_array);
	json_object_object_add(obj, "colors", colors_array);
#endif

	/* return string to user */
	*str = strdup(json_object_to_json_string(obj));
	json_object_put(obj);

	return 1;
}
