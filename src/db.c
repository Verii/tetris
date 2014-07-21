#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>

#include "db.h"
#include "db_statements.h"
#include "debug.h"
#include "blocks.h"

static int
db_open (struct db_info *entry)
{
	if (!entry || !entry->file_loc)
		return -1;

	log_info ("Opening database %s", entry->file_loc);

	int status = sqlite3_open (entry->file_loc, &entry->db);
	if (status != SQLITE_OK) {
		log_err ("DB cannot be opened. Error occured (%d)", status);
		return -1;
	}

	return 1;
}

static void
db_close (struct db_info *entry)
{
	log_info ("Closing database %s", entry->file_loc);
	sqlite3_close (entry->db);
}

int
db_save_score (struct db_info *entry, const struct block_game *pgame)
{
	sqlite3_stmt *stmt;

	if (!pgame->id || pgame->score == 0)
		return 0;

	log_info ("Trying to insert scores to database");

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	sqlite3_prepare_v2 (entry->db, create_scores,
			sizeof create_scores, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	char *insert;
	int ret = asprintf (&insert, insert_scores,
		pgame->id, pgame->level, pgame->score, (uint64_t)time(NULL));

	if (ret < 0) {
		log_err ("Out of memory");
	} else {
		sqlite3_prepare_v2 (entry->db, insert, strlen(insert),
			&stmt, NULL);
		free (insert);
		sqlite3_step (stmt);
		sqlite3_finalize (stmt);
	}

	db_close (entry);

	return 1;
}

int
db_save_state (struct db_info *entry, const struct block_game *pgame)
{
	sqlite3_stmt *stmt;

	log_info ("Trying to save state to database");

	if (db_open (entry) < 0)
		return 0;

	/* Create table if it doesn't exist */
	sqlite3_prepare_v2 (entry->db, create_state,
			sizeof create_state, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	debug ("Saving game spaces");

	char *insert;
	int ret = asprintf (&insert, insert_state,
		pgame->id, pgame->score, pgame->lines_destroyed,
		pgame->level, pgame->mod, (uint64_t) time (NULL),
		pgame->width, pgame->height);

	if (ret < 0) {
		log_err ("Out of memory");
	} else {
		unsigned char data_len = 0;
		unsigned char *data;

		data_len = (BLOCKS_ROWS-2) * sizeof(*pgame->spaces);
		data = malloc (data_len);

		if (data == NULL) {
			/* Non-fatal, we just don't save to database */
			log_err ("Unable to acquire memory, %d", data_len);
			goto mem_err;
		}

		memcpy (data, &pgame->spaces[2], data_len);
		sqlite3_prepare_v2 (entry->db, insert, strlen (insert),
				&stmt, NULL);
		free (insert);

		/* NOTE sqlite will free the @data block for us */
		sqlite3_bind_blob (stmt, 1, data, data_len, free);
		sqlite3_step (stmt);
		sqlite3_finalize (stmt);
	}

	db_close (entry);
	return 1;

mem_err:
	db_close (entry);
	return 0;
}

/* Queries database for newest game state information and copies it to pgame.
 */
int
db_resume_state (struct db_info *entry, struct block_game *pgame)
{
	int ret;
	sqlite3_stmt *stmt;

	log_info ("Trying to restore saved game");

	if (db_open (entry) < 0)
		return -1;

	/* Look for newest entry in table */
	sqlite3_prepare_v2 (entry->db, select_state,
			sizeof select_state, &stmt, NULL);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		strncpy (pgame->id, (const char *)
			sqlite3_column_text (stmt, 0), sizeof pgame->id);

		pgame->score = sqlite3_column_int (stmt, 1);
		pgame->lines_destroyed = sqlite3_column_int (stmt, 2);
		pgame->level = sqlite3_column_int (stmt, 3);
		pgame->mod = sqlite3_column_int (stmt, 4);

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

	log_info ("Level = %d, Score = %d, Difficulty = %d, (%d, %d)",
		pgame->level, pgame->score, pgame->mod,
		pgame->width, pgame->height);

	int rowid;
	sqlite3_prepare_v2 (entry->db, select_state_rowid,
			sizeof select_state_rowid, &stmt, NULL);

	if (sqlite3_step (stmt) == SQLITE_ROW) {
		rowid = sqlite3_column_int (stmt, 0);

		/* delete from table */
		sqlite3_stmt *delete;
		sqlite3_prepare_v2 (entry->db, delete_state_rowid,
			sizeof delete_state_rowid, &delete, NULL);

		sqlite3_bind_int (delete, 1, rowid);
		sqlite3_step (delete);
		sqlite3_finalize (delete);
	}

	sqlite3_finalize (stmt);

	db_close (entry);
	return ret;
}

/* Returns a pointer to the first element of a tail queue, of @results length.
 * This list can be iterated over to extract the fields:
 * name, level, score, date.
 *
 * NOTE Be sure to call db_clean_scores after this to free memory.
 */
struct db_results *
db_get_scores (struct db_info *entry, int results)
{
	sqlite3_stmt *stmt;

	debug ("Getting (%d) hs from db", results);

	if (db_open (entry) < 0)
		return NULL;

	sqlite3_prepare_v2 (entry->db, select_scores,
			sizeof select_scores, &stmt, NULL);
	TAILQ_INIT (&results_head);

	while (results-- > 0 && sqlite3_step (stmt) == SQLITE_ROW) {

		if (sqlite3_column_count (stmt) != 4)
			/* improperly formatted row */
			break;

		struct db_results *np = malloc (sizeof *np);

		memset (np->id, 0, sizeof np->id);
		strncpy (np->id, (const char *)
				sqlite3_column_text (stmt, 0), sizeof np->id);
		np->id[sizeof(np->id) -1] = '\0';

		np->level= sqlite3_column_int (stmt, 1);
		np->score = sqlite3_column_int (stmt, 2);
		np->date = sqlite3_column_int (stmt, 3);

		if (!np->score || !np->level || !np->date) {
			free (np);
			break;
		}

		TAILQ_INSERT_TAIL (&results_head, np, entries);
	}

	sqlite3_finalize (stmt);
	db_close (entry);

	return results_head.tqh_first;
}
