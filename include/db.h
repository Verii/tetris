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

#ifndef DB_H_
#define DB_H_

#include <stdlib.h>
#include <sqlite3.h>

#include "tetris.h"

int db_save_score(tetris *);
int db_save_state(tetris *);
int db_resume_state(tetris *);

/* Get an array of tetris struct pointers containing the data fields of the
 * game save states.
 * - level
 * - score
 * - date
 * - id
 */
int db_get_scores(tetris *, tetris **, size_t);

/* Remove entries in linked list */
void db_clean_scores(tetris **, size_t);

#endif /* DB_H_ */
