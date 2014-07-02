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
	int status;
	if (entry == NULL)
		return -1;

	status = sqlite3_open (entry->file_loc, &entry->db);
	log_info ("Opening database %s", entry->file_loc);

	if (status != SQLITE_OK) {
		log_err ("Error occured: %d", status);
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
	char *statement;
	time_t t = time (NULL);

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	asprintf (&statement, "CREATE TABLE Scores (name TEXT, "
			"level INTEGER, score INTEGER, date INTEGER);");
	if (statement == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, statement,
			strlen (statement), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (statement);

	/* Write scores into database */
	asprintf (&statement, "INSERT INTO Scores (name, level, score, date)"
			" VALUES ( \"%s\", %d, %d, %d );", entry->id,
			pgame->level, pgame->score, (uint32_t) t);
	if (statement == NULL) {
		log_err ("Out of memory");
		exit (2);
	}
	sqlite3_prepare_v2 (entry->db, statement,
			strlen (statement), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (statement);

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
//	struct db_results *np;
//	sqlite3_stmt *stmt;
	(void) results;
	LIST_INIT (&results_head);

	if (entry == NULL) {
		log_warn ("Unable to access database");
		return NULL;
	}

	db_open (entry);

	/* XXX */
#if 0
	/* Get top 10 high scores */
	while (1) {
		np = calloc (1, sizeof *np);
		LIST_INSERT_HEAD (&results_head, np, entries);
	}
#endif

	db_close (entry);

	return results_head.lh_first;
}

void
db_clean_scores (void)
{
	while (results_head.lh_first)
		LIST_REMOVE (results_head.lh_first, entries);
}
