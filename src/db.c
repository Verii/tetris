#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#include "db.h"
#include "db_statements.h"
#include "debug.h"
#include "blocks.h"

static sqlite3 *db;

int
db_init (const char *file)
{
	if (!file)
		return -1;

	debug ("Opening database %s", file);

	int status = sqlite3_open (file, &db);
	if (status != SQLITE_OK) {
		log_err ("DB cannot be opened. Error occured (%d)", status);
		return -1;
	}

	return 1;
}

void
db_clean (void)
{
	sqlite3_close (db);
}

int
db_save_score (void)
{
	sqlite3_stmt *stmt;

	if (!pgame->id || pgame->score == 0)
		return 0;

	log_info ("Trying to insert scores to database");

	/* Make sure the db has the proper tables */
	sqlite3_prepare_v2 (db, create_scores,
			sizeof create_scores, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	const int insert_len = sizeof (char) * 128;
	char *insert = malloc (insert_len);

	if (insert == NULL) {
		log_err ("Out of memory");
	} else {
		snprintf (insert, insert_len, insert_scores,
			pgame->id, pgame->level,
			pgame->score, time(NULL));

		sqlite3_prepare_v2 (db, insert, strlen(insert),
			&stmt, NULL);
		free (insert);
		sqlite3_step (stmt);
		sqlite3_finalize (stmt);
	}

	return 1;
}

int
db_save_state (void)
{
	sqlite3_stmt *stmt;

	log_info ("Trying to save state to database");

	/* Create table if it doesn't exist */
	sqlite3_prepare_v2 (db, create_state,
			sizeof create_state, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	debug ("Saving game spaces");

	const int insert_len = sizeof (char) * 256;
	char *insert = malloc (insert_len);

	if (insert == NULL) {
		log_err ("Out of memory");
	} else {

		snprintf (insert, insert_len, insert_state,
			pgame->id, pgame->score, pgame->lines_destroyed,
			pgame->level, pgame->diff, time (NULL),
			pgame->width, pgame->height);

		sqlite3_prepare_v2 (db, insert, strlen (insert),
				&stmt, NULL);
		free (insert);

		uint16_t data[BLOCKS_ROWS-2];
		memcpy (data, &pgame->spaces[2], sizeof data);

		sqlite3_bind_blob (stmt, 1, data, sizeof data, SQLITE_STATIC);
		sqlite3_step (stmt);
		sqlite3_finalize (stmt);
	}

	return 1;
}

/* Queries database for newest game state information and copies it to pgame.
 */
int
db_resume_state (void)
{
	int ret;
	sqlite3_stmt *stmt;

	log_info ("Trying to restore saved game");

	/* Look for newest entry in table */
	sqlite3_prepare_v2 (db, select_state,
			sizeof select_state, &stmt, NULL);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		strncpy (pgame->id, (const char *)
			sqlite3_column_text (stmt, 0), sizeof pgame->id);

		pgame->score = sqlite3_column_int (stmt, 1);
		pgame->lines_destroyed = sqlite3_column_int (stmt, 2);
		pgame->level = sqlite3_column_int (stmt, 3);
		pgame->diff = sqlite3_column_int (stmt, 4);

		pgame->width = sqlite3_column_int (stmt, 6);
		pgame->height = sqlite3_column_int (stmt, 7);

		const char *blob = sqlite3_column_blob (stmt, 8);
		memcpy (&pgame->spaces[2], &blob[0],
			(BLOCKS_ROWS-2) * sizeof(*pgame->spaces));

		/* Set block spaces to random colors, we don't really need to
		 * save them ... */
		for (int i = 0; i < BLOCKS_ROWS; i++)
			for (int j = 0; j < BLOCKS_COLUMNS; j++)
				pgame->colors[i][j] = rand();
		ret = 1;
	} else {
		log_warn ("No game saves found");
		ret = -1;
	}

	sqlite3_finalize (stmt);

	debug ("Level = %d, Score = %d, Difficulty = %d, (%d, %d)",
		pgame->level, pgame->score, pgame->diff,
		pgame->width, pgame->height);

	int rowid;
	sqlite3_prepare_v2 (db, select_state_rowid,
			sizeof select_state_rowid, &stmt, NULL);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		rowid = sqlite3_column_int (stmt, 0);

		/* delete from table */
		sqlite3_stmt *delete;
		sqlite3_prepare_v2 (db, delete_state_rowid,
			sizeof delete_state_rowid, &delete, NULL);

		sqlite3_bind_int (delete, 1, rowid);
		sqlite3_step (delete);
		sqlite3_finalize (delete);
	}

	sqlite3_finalize (stmt);

	return ret;
}

/* Returns a pointer to the first element of a tail queue, of @results length.
 * This list can be iterated over to extract the fields:
 * name, level, score, date.
 *
 * NOTE Be sure to call db_clean_scores after this to free memory.
 */
struct db_save *
db_get_scores (int results)
{
	sqlite3_stmt *stmt;

	debug ("Getting (%d) hs from db", results);

	sqlite3_prepare_v2 (db, select_scores,
			sizeof select_scores, &stmt, NULL);

	TAILQ_INIT (&save_head);

	while (results-- > 0 && sqlite3_step (stmt) == SQLITE_ROW) {

		if (sqlite3_column_count (stmt) != 4)
			/* improperly formatted row */
			break;

		struct db_save *np = malloc (sizeof *np);

		strncpy (np->save.id, (const char *)
			sqlite3_column_text (stmt, 0), sizeof np->save.id);
		np->save.id[sizeof(np->save.id) -1] = '\0';

		np->save.level= sqlite3_column_int (stmt, 1);
		np->save.score = sqlite3_column_int (stmt, 2);
		np->date = sqlite3_column_int (stmt, 3);

		if (!np->save.score || !np->save.level) {
			free (np);
			break;
		}

		TAILQ_INSERT_TAIL (&save_head, np, entries);
	}

	sqlite3_finalize (stmt);

	return save_head.tqh_first;
}

struct db_save *
db_get_states (void)
{
	debug ("Getting saves states from db");

	sqlite3_stmt *stmt;
	sqlite3_prepare_v2 (db, select_state, sizeof select_state, &stmt, NULL);
	TAILQ_INIT (&save_head);

	while (sqlite3_step (stmt) == SQLITE_ROW) {

		struct db_save *np = malloc (sizeof *np);

		strncpy (np->save.id, (const char *)
			sqlite3_column_text (stmt, 0), sizeof np->save.id);

		np->save.score = sqlite3_column_int (stmt, 1);
		np->save.lines_destroyed= sqlite3_column_int (stmt, 2);
		np->save.level= sqlite3_column_int (stmt, 3);
		np->save.diff= sqlite3_column_int (stmt, 4);
		np->date = sqlite3_column_int (stmt, 5);
		np->save.width= sqlite3_column_int (stmt, 6);
		np->save.height= sqlite3_column_int (stmt, 7);

		const char *blob = sqlite3_column_blob (stmt, 8);
		memcpy (&np->save.spaces[2], &blob[0],
			(BLOCKS_ROWS-2) * sizeof(*np->save.spaces));

		TAILQ_INSERT_TAIL (&save_head, np, entries);
	}

	sqlite3_finalize (stmt);

	return save_head.tqh_first;
}
