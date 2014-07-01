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

	int status = sqlite3_open (entry->file_loc, &entry->db);
	log_info ("Opening database %s\n", entry->file_loc);

	if (status != SQLITE_OK) {
		log_err ("Error occured: %d", status);
		return -1;
	}

	return 1;
}

int
db_add_game_score (struct db_info *entry, struct block_game *pgame)
{
	sqlite3_stmt *stmt;
	char *create, *insert;
	time_t t = time (NULL);

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	asprintf (&create, "CREATE TABLE Scores (name TEXT, "
			"level INTEGER, score INTEGER, date INTEGER);");
	if (create == NULL) {
		log_err ("%s", "Out of memory");
		exit (2);
	}

	asprintf (&insert, "INSERT INTO Scores VALUES ( "", %d, %d, %d );",
			pgame->level, pgame->score, (int) t);
	if (insert == NULL) {
		log_err ("%s", "Out of memory");
		exit (2);
	}

	sqlite3_prepare_v2 (entry->db, create, strlen (create), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (create);

	sqlite3_prepare_v2 (entry->db, insert, strlen (insert), &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);
	free (insert);

	sqlite3_close_v2 (entry->db);

	return 1;
}

int
db_add_game_state (struct db_info *entry, struct block_game *pgame)
{
	(void) entry;
	(void) pgame;
	return -1;
}

struct db_results *
db_get_scores (struct db_info *entry, int *results)
{
	int attempt_res = 10;
	LIST_INIT (&results_head);

	// db_open (entry);
	// sqlite3_close_v2 (entry->db);

	if (entry == NULL) {
		log_warn ("%s", "Unable to access database");
		*results = -1;
		return NULL;
	}

	if (results != NULL && *results > 0) {
		attempt_res = *results;
	}

	struct db_results *np = calloc (1, sizeof *np);
	LIST_INSERT_HEAD (&results_head, np, entries);

	if (results)
		*results = attempt_res;

	return results_head.lh_first;
}

void
db_clean_scores (void)
{
	while (results_head.lh_first)
		LIST_REMOVE (results_head.lh_first, entries);
}
