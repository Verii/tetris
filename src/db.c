#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <time.h>

#include "db.h"
#include "debug.h"
#include "blocks.h"

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
		log_err ("Error occured, %d", status);
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
	char *state;

	if (!pgame->level || !pgame->score)
		return 0;

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	asprintf (&state, "CREATE TABLE Scores (name TEXT, "
			"level INT, score INT, date INT);");
	if (state == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, state, strlen (state), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (state);

	/* Write scores into database */
	asprintf (&state,
		"INSERT INTO Scores (name, level, score, date) "
		"VALUES ( \"%s\", %d, %d, %ld );",
		entry->id, pgame->level, pgame->score, (uint64_t)time(NULL));
	if (state == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, state, strlen (state), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (state);

	db_close (entry);

	return 1;
}

int
db_save_state (struct db_info *entry, struct block_game *pgame)
{
	char *state;
	sqlite3_stmt *stmt;
	int len = BLOCKS_ROWS * BLOCKS_COLUMNS;
	uint8_t *ba = malloc (len);

	if (entry == NULL || entry->file_loc == NULL)
		return -1;

	if (pgame == NULL)
		return -1;

	if (ba == NULL) {
		log_err ("Out of memory");
		exit (2);
	}

	db_open (entry);

	asprintf (&state,
		"CREATE TABLE State (name TEXT, score INT, lines INT, "
		"level INT, diff INT, date INT, spaces BLOB, colors BLOB);");

	if (state == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, state, strlen (state), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (state);

	asprintf (&state,
		"INSERT INTO State "
		"VALUES (\"%s\", %d, %d, %d, %d, %ld, ?, ?);",
		entry->id, pgame->score, pgame->lines_destroyed,
		pgame->level, pgame->mod, (uint64_t) time (NULL));

	if (state == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, state, strlen (state), &stmt, NULL);

	for (size_t i = 0; i < BLOCKS_ROWS; i++)
		memcpy (ba+(i*BLOCKS_COLUMNS), pgame->spaces[i],
				BLOCKS_COLUMNS);
	sqlite3_bind_blob (stmt, 1, ba, len, SQLITE_STATIC);

	for (size_t i = 0; i < BLOCKS_ROWS; i++)
		memcpy (ba+(i*BLOCKS_COLUMNS), pgame->colors[i],
				BLOCKS_COLUMNS);
	sqlite3_bind_blob (stmt, 2, ba, len, SQLITE_STATIC);

	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free(state);
	free (ba);

	db_close (entry);

	return 1;
}

int
db_resume_state (struct db_info *entry, struct block_game *pgame)
{
	sqlite3_stmt *stmt;

	if (entry == NULL || entry->file_loc == NULL)
		return -1;

	if (pgame == NULL)
		return -1;

	db_open (entry);

	const char *select = "SELECT * FROM State ORDER BY date DESC;";
	sqlite3_prepare_v2 (entry->db, select, strlen (select), &stmt, NULL);

	if (sqlite3_step (stmt) != SQLITE_ROW)
		return 0;

	/* name, score, lines, level, diff, date, spaces, colors */
	if (sqlite3_column_count (stmt) < 8)
		return 0;

	int len = sqlite3_column_bytes (stmt, 0) +1;
	pgame->name = malloc (len);

	strncpy (pgame->name,
		 (const char *)sqlite3_column_text (stmt, 0), len);

	pgame->score = sqlite3_column_int (stmt, 1);
	pgame->lines_destroyed = sqlite3_column_int (stmt, 2);
	pgame->level = sqlite3_column_int (stmt, 3);
	pgame->mod = sqlite3_column_int (stmt, 4);

	len = BLOCKS_COLUMNS * BLOCKS_ROWS;
	uint8_t *ba = malloc (len);

	/* Copy block spaces back */
	memcpy (ba, sqlite3_column_blob (stmt, 6), len);
	for (size_t i = 0; i < BLOCKS_ROWS; i++) {
		pgame->spaces[i] = malloc (BLOCKS_COLUMNS);
		memcpy (pgame->spaces[i], ba+(i*BLOCKS_COLUMNS),
				BLOCKS_COLUMNS);
	}

	memcpy (ba, sqlite3_column_blob (stmt, 7), len);
	for (size_t i = 0; i < BLOCKS_ROWS; i++) {
		pgame->colors[i] = malloc (BLOCKS_COLUMNS);
		memcpy (pgame->colors[i], ba+(i*BLOCKS_COLUMNS),
				BLOCKS_COLUMNS);
	}

	free (ba);
	sqlite3_finalize (stmt);

	const char *drop = "DROP TABLE State;";
	sqlite3_prepare_v2 (entry->db, drop, strlen (drop), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	db_close (entry);

	return 1;
}

struct db_results *
db_get_scores (struct db_info *entry, int results)
{
	sqlite3_stmt *stmt;

	if (entry == NULL)
		return NULL;

	db_open (entry);
	TAILQ_INIT (&results_head);

	const char *select = "SELECT * FROM Scores ORDER BY score DESC;";
	sqlite3_prepare_v2 (entry->db, select, strlen (select), &stmt, NULL);

	while (results-- > 0 && sqlite3_step (stmt) == SQLITE_ROW) {
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

		int len = sqlite3_column_bytes (stmt, 0) +1;
		np->id = calloc (len, 1);
		strncpy (np->id,
			 (const char *) sqlite3_column_text (stmt, 0), len);

		TAILQ_INSERT_TAIL (&results_head, np, entries);
	}

	sqlite3_finalize (stmt);
	db_close (entry);

	return results_head.tqh_first;
}

void
db_clean_scores (void)
{
	while (results_head.tqh_first) {
		free (results_head.tqh_first->id);
		TAILQ_REMOVE (&results_head, results_head.tqh_first, entries);
	}
}
