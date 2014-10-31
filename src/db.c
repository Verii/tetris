/*
 * Copyright (C) 2014  James Smith <james@theta.pw>
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

#include <bsd/string.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#include "db.h"
#include "debug.h"
#include "blocks.h"

/* Scores: name, level, score, date */
const char create_scores[] =
	"CREATE TABLE Scores(name TEXT,level INT,score INT,date INT);";

const char insert_scores[] =
	"INSERT INTO Scores VALUES(\"%s\",%d,%d,%lu);";

const char select_scores[] =
	"SELECT * FROM Scores ORDER BY score DESC;";

/* State: name, score, lines, level, date, spaces */
const char create_state[] =
	"CREATE TABLE State(name TEXT,score INT,lines INT,level INT,"
	"date INT,spaces BLOB);";

const char insert_state[] =
	"INSERT INTO State VALUES(\"%s\",%d,%d,%d,%lu,?);";

const char select_state[] =
	"SELECT * FROM State ORDER BY date DESC;";

/* Find the entry we just pulled from the database.
 * There's probably a simpler way than using two SELECT calls, but I'm a total
 * SQL noob, so ... */
const char select_state_rowid[] =
	"SELECT ROWID,date FROM State ORDER BY date DESC;";

/* Remove the entry pulled from the database. This lets us have multiple saves
 * in the database concurently.
 */
const char delete_state_rowid[] =
	"DELETE FROM State WHERE ROWID = ?;";

#define db_close(entry) do { \
	log_info("Closing database %s", (entry)->file_loc); \
	sqlite3_close((entry)->db); \
	} while (0)

static int db_open(struct db_info *entry)
{
	int status;

	if (!entry || !entry->file_loc)
		return -1;

	log_info("Opening database %s", entry->file_loc);

	status = sqlite3_open(entry->file_loc, &entry->db);
	if (status != SQLITE_OK) {
		log_err("DB cannot be opened. Error occured (%d)", status);
		return -1;
	}

	return 1;
}

int db_save_score(struct db_info *entry, const struct blocks_game *pgame)
{
	sqlite3_stmt *stmt;
	char *insert = NULL;
	int len = -1;

	if (!entry->id || pgame->score == 0)
		return 0;

	log_info("Trying to insert scores to database");

	if (db_open(entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	sqlite3_prepare_v2(entry->db, create_scores,
			   sizeof create_scores, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	len = asprintf(&insert, insert_scores,
			   entry->id, pgame->level, pgame->score,
			   (uint64_t) time(NULL));

	if (len < 0) {
		log_err("Out of memory");
	} else {
		sqlite3_prepare_v2(entry->db, insert, strlen(insert),
				   &stmt, NULL);
		free(insert);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);
	}

	db_close(entry);

	return 1;
}

int db_save_state(struct db_info *entry, const struct blocks_game *pgame)
{
	sqlite3_stmt *stmt;
	char *insert, *data = NULL;
	int len, ret = 0, data_len = 0;

	if (db_open(entry) < 0)
		return 0;

	/* Create table if it doesn't exist */
	sqlite3_prepare_v2(entry->db, create_state,
			   sizeof create_state, &stmt, NULL);
	sqlite3_step(stmt);
	sqlite3_finalize(stmt);

	len = asprintf(&insert, insert_state,
			   entry->id, pgame->score, pgame->lines_destroyed,
			   pgame->level, (uint64_t) time(NULL));

	if (len < 0) {
		log_err("Out of memory");
	} else {
		data_len = (BLOCKS_MAX_ROWS - 2) * sizeof(*pgame->spaces);
		data = malloc(data_len);

		if (!data) {
			/* Non-fatal, we just don't save to database */
			log_err("Unable to acquire memory, %d bytes", data_len);
			goto error;
		}

		memcpy(data, &pgame->spaces[2], data_len);
		sqlite3_prepare_v2(entry->db, insert, strlen(insert),
				   &stmt, NULL);

		/* NOTE sqlite will free the @data block for us */
		sqlite3_bind_blob(stmt, 1, data, data_len, free);
		sqlite3_step(stmt);
		sqlite3_finalize(stmt);

		ret = 1;
	}

 error:
	if (len > 0)
		free(insert);
	db_close(entry);
	return ret;
}

/* Queries database for newest game state information and copies it to pgame.
 */
int db_resume_state(struct db_info *entry, struct blocks_game *pgame)
{
	sqlite3_stmt *stmt, *delete;
	int ret, rowid, i, j;
	const char *blob;

	if (db_open(entry) < 0)
		return -1;

	log_info("Trying to restore saved game");

	/* Look for newest entry in table */
	sqlite3_prepare_v2(entry->db, select_state,
			   sizeof select_state, &stmt, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		strlcpy(entry->id, (const char *)
			sqlite3_column_text(stmt, 0), sizeof entry->id);

		pgame->score = sqlite3_column_int(stmt, 1);
		pgame->lines_destroyed = sqlite3_column_int(stmt, 2);
		pgame->level = sqlite3_column_int(stmt, 3);

		blob = sqlite3_column_blob(stmt, 5);
		memcpy(&pgame->spaces[2], &blob[0],
		       (BLOCKS_MAX_ROWS - 2) * sizeof(*pgame->spaces));

		/* Set block spaces to random colors, we don't really need to
		 * save them ... */
		for (i = 0; i < BLOCKS_MAX_ROWS; i++)
			for (j = 0; j < BLOCKS_MAX_COLUMNS; j++)
				pgame->colors[i][j] = rand();
		ret = 1;
	} else {
		log_warn("No game saves found");
		ret = -1;
	}

	sqlite3_finalize(stmt);

	sqlite3_prepare_v2(entry->db, select_state_rowid,
			   sizeof select_state_rowid, &stmt, NULL);

	if (sqlite3_step(stmt) == SQLITE_ROW) {
		rowid = sqlite3_column_int(stmt, 0);

		/* delete from table */
		sqlite3_prepare_v2(entry->db, delete_state_rowid,
				   sizeof delete_state_rowid, &delete, NULL);

		sqlite3_bind_int(delete, 1, rowid);
		sqlite3_step(delete);
		sqlite3_finalize(delete);
	}

	sqlite3_finalize(stmt);

	db_close(entry);
	return ret;
}

/* Returns a pointer to the first element of a tail queue, of @results length.
 * This list can be iterated over to extract the fields:
 * name, level, score, date.
 *
 * NOTE Be sure to call db_clean_scores after this to free memory.
 */
struct db_results *db_get_scores(struct db_info *entry, size_t results)
{
	sqlite3_stmt *stmt;
	struct db_results *np;

	if (db_open(entry) < 0)
		return NULL;

	sqlite3_prepare_v2(entry->db, select_scores,
			   sizeof select_scores, &stmt, NULL);
	TAILQ_INIT(&results_head);

	while (results-- > 0 && sqlite3_step(stmt) == SQLITE_ROW) {
		if (sqlite3_column_count(stmt) != 4)
			break; /* improperly formatted row */

		np = malloc(sizeof *np);
		if (!np) {
			log_warn("Out of memory, %d bytes.", sizeof *np);
			break;
		}

		memset(np->id, 0, sizeof np->id);
		strlcpy(np->id, (const char *)
			sqlite3_column_text(stmt, 0), sizeof np->id);
		np->id[sizeof(np->id) - 1] = '\0';

		np->level = sqlite3_column_int(stmt, 1);
		np->score = sqlite3_column_int(stmt, 2);
		np->date = sqlite3_column_int(stmt, 3);

		if (!np->score || !np->level || !np->date) {
			free(np);
			break;
		}

		TAILQ_INSERT_TAIL(&results_head, np, entries);
		np = NULL;
	}

	sqlite3_finalize(stmt);
	db_close(entry);

	return results_head.tqh_first;
}
