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

#include <stdint.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <time.h>

#include "blocks.h"

int db_init(const char *path);
void db_cleanup(void);

/* Saves game state to disk. Can be restored at a later time */
int db_save_state(struct blocks_game *);
int db_resume_state(struct blocks_game *);

/* Save game score to disk when the player loses a game */
int db_save_score(struct blocks_game *);

/* Linked list returned to user after db_get_scores() call */
TAILQ_HEAD(db_results_head, db_results) results_head;
struct db_results {
	char id[16];
	uint32_t score;
	uint8_t level;
	time_t date;
	TAILQ_ENTRY(db_results) entries;
};

/* Returns a linked list to (n) highscores in the database */
int db_get_scores(struct db_results **res, size_t n);

/* Cleanup memory */
void db_clean_scores(void);

#endif	/* DB_H_ */
