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
	time_t t = time (NULL);

	if (!pgame->level || !pgame->score)
		return 0;

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	asprintf (&state, "CREATE TABLE Scores (name TEXT, "
			"level INTEGER, score INTEGER, date INTEGER);");
	if (state == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, state, strlen (state), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (state);

	/* Write scores into database */
	asprintf (&state, "INSERT INTO Scores (name, level, score, date)"
			" VALUES ( \"%s\", %d, %d, %d );", entry->id,
			pgame->level, pgame->score, (uint32_t) t);
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
	(void) entry;
	(void) pgame;
	return -1;
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

		np->level = sqlite3_column_int (stmt, 1);
		np->score = sqlite3_column_int (stmt, 2);
		np->date = sqlite3_column_int (stmt, 3);

		if (!np->score || !np->level || !np->date) {
			free (np);
			break;
		}

		/* name, level, score, date */
		int len = sqlite3_column_bytes (stmt, 0) +1;
		np->id = calloc (len, 1);
		strncpy (np->id, sqlite3_column_text (stmt, 0), len);

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
