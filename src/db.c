#define _GNU_SOURCE

#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "db.h"
#include "debug.h"
#include "blocks.h"

static const char create_scores[] = "CREATE TABLE Scores (name TEXT, level INT, "
		"score INT, date INT);";

static const char create_state[] = "CREATE TABLE State (name TEXT, score INT, "
		"lines INT, level INT, diff INT, date INT, spaces BLOB, "
		"colors BLOB);";

static int
db_open (struct db_info *entry)
{
	if (entry == NULL)
		return -1;

	if (entry->file_loc == NULL)
		return 0;

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
	sqlite3_close_v2 (entry->db);
}

int
db_save_score (struct db_info *entry, struct block_game *pgame)
{
	sqlite3_stmt *stmt;
	char *insert;
	int ret;

	log_info ("Trying to insert scores to database");

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	sqlite3_prepare_v2 (entry->db, create_scores,
			sizeof create_scores, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	ret = asprintf (&insert,
		"INSERT INTO Scores (name, level, score, date) "
		"VALUES ( \"%s\", %d, %d, %lu );",
		entry->id, pgame->level, pgame->score, (uint64_t)time(NULL));

	if (ret < 0) {
		log_err ("Out of memory");
		db_close (entry);
		return ret;
	}

	sqlite3_prepare_v2 (entry->db, insert, strlen(insert), &stmt, NULL);
	free (insert);

	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	db_close (entry);

	return 1;
}

int
db_save_state (struct db_info *entry, struct block_game *pgame)
{
	sqlite3_stmt *stmt;
	char *insert;
	int ret;

	log_info ("Trying to save state to database");

	if (db_open (entry) < 0)
		return 0;

	/* Create table if it doesn't exist */
	sqlite3_prepare_v2 (entry->db, create_state,
			sizeof create_state, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	/* name, score, lines, level, diff, date, spaces, colors */
	ret = asprintf (&insert,
		"INSERT INTO State "
		"VALUES (\"%s\", %d, %d, %d, %d, %lu, ?, ?);",
		entry->id, pgame->score, pgame->lines_destroyed,
		pgame->level, pgame->mod, (uint64_t) time (NULL));

	if (ret < 0) {
		log_err ("Out of memory");
		db_close (entry);
		return ret;
	}

	sqlite3_prepare_v2 (entry->db, insert, strlen (insert), &stmt, NULL);
	free (insert);

	/* Add binary blobs to INSERT statement */
	debug ("Saving game spaces");
	char *data = malloc (BLOCKS_COLUMNS * BLOCKS_ROWS);
	if (data == NULL) {
		log_err ("Out of memory");
		sqlite3_finalize (stmt);
		db_close (entry);
		return -1;
	}
	for (int i = 0; i < BLOCKS_ROWS * BLOCKS_COLUMNS; i++)
		data[i] =
		pgame->spaces[i/BLOCKS_COLUMNS][i%BLOCKS_COLUMNS];

	sqlite3_bind_blob (stmt, 1, data, 220, free);

	debug ("Saving game colors");
	data = malloc (BLOCKS_COLUMNS * BLOCKS_ROWS);
	if (data == NULL) {
		log_err ("Out of memory");
		sqlite3_finalize (stmt);
		db_close (entry);
		return -1;
	}

	for (int i = 0; i < BLOCKS_ROWS * BLOCKS_COLUMNS; i++)
		data[i] =
		pgame->colors[i/BLOCKS_COLUMNS][i%BLOCKS_COLUMNS];
	sqlite3_bind_blob (stmt, 2, data, 220, free);

	/* Commit, and cleanup */
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	db_close (entry);

	return 1;
}

int
db_resume_state (struct db_info *entry, struct block_game *pgame)
{
	sqlite3_stmt *stmt;

	log_info ("Trying to restore saved game");

	if (db_open (entry) < 0)
		return -1;

	/* Get freshest entry in table */
	const char select[] = "SELECT * FROM State ORDER BY date DESC;";
	sqlite3_prepare_v2 (entry->db, select, sizeof select, &stmt, NULL);

	/* Quit if old game-save doesn't exist */
	if (sqlite3_step (stmt) != SQLITE_ROW) {
		log_info ("Old game saves not found");
		sqlite3_finalize (stmt);
		db_close (entry);
		return 0;
	}

	strncpy (entry->id, (const char *)
			sqlite3_column_text (stmt, 0), sizeof entry->id);

	pgame->score = sqlite3_column_int (stmt, 1);
	pgame->lines_destroyed = sqlite3_column_int (stmt, 2);
	pgame->level = sqlite3_column_int (stmt, 3);
	pgame->mod = sqlite3_column_int (stmt, 4);

	debug ("Restoring game spaces");
	const char *blob = sqlite3_column_blob (stmt, 6);
	for (int i = 0; i < BLOCKS_COLUMNS * BLOCKS_ROWS; i++)
		pgame->spaces[i/BLOCKS_COLUMNS][i%BLOCKS_COLUMNS] = blob[i];

	debug ("Restoring game colors");
	blob = sqlite3_column_blob (stmt, 7);
	for (int i = 0; i < BLOCKS_COLUMNS * BLOCKS_ROWS; i++)
		pgame->colors[i/BLOCKS_COLUMNS][i%BLOCKS_COLUMNS] = blob[i];

	sqlite3_finalize (stmt);

	/* Remove old entries to DB */
	const char drop[] = "DROP TABLE State;";
	sqlite3_prepare_v2 (entry->db, drop, sizeof drop, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	db_close (entry);

	return 1;
}

struct db_results *
db_get_scores (struct db_info *entry, int results)
{
	sqlite3_stmt *stmt;

	log_info ("Trying to get (%d) highscores from database", results);

	if (db_open (entry) < 0)
		return NULL;

	TAILQ_INIT (&results_head);

	const char select[] = "SELECT name,level,score,date "
	       "FROM Scores ORDER BY score DESC;";
	sqlite3_prepare_v2 (entry->db, select, sizeof select, &stmt, NULL);

	/* Get (results) entries */
	while (results-- > 0 && sqlite3_step (stmt) == SQLITE_ROW) {
		/* improperly formatted row */
		if (sqlite3_column_count (stmt) < 4)
			break;

		struct db_results *np;
		np = calloc (1, sizeof *np);

		/* name, level, score, date */
		np->level = sqlite3_column_int (stmt, 1);
		np->score = sqlite3_column_int (stmt, 2);
		np->date = sqlite3_column_int (stmt, 3);

		if (!np->score || !np->level || !np->date) {
			free (np);
			break;
		}

		strncpy (np->id, (const char *)
				sqlite3_column_text (stmt, 0), sizeof np->id);

		TAILQ_INSERT_TAIL (&results_head, np, entries);
	}

	sqlite3_finalize (stmt);
	db_close (entry);

	return results_head.tqh_first;
}

void
db_clean_scores (void)
{
	while (results_head.tqh_first)
		TAILQ_REMOVE (&results_head, results_head.tqh_first, entries);
}
