#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "db.h"
#include "debug.h"
#include "blocks.h"

/* Scores: name, score, diff, date */
const char create_scores[] =
	"CREATE TABLE Scores(name TEXT,level INT,score INT,date INT);";

const char insert_scores[] =
	"INSERT INTO Scores VALUES(\"%s\",%d,%d,%lu);";

const char select_scores[] =
	"SELECT * FROM Scores ORDER BY score DESC;";

/* State: name, score, lines, level, diff, date, spaces */
const char create_state[] =
	"CREATE TABLE State(name TEXT,score INT,lines INT,level INT,"
	"diff INT,date INT,width INT,height INT,spaces BLOB);";

const char insert_state[] =
	"INSERT INTO State VALUES(\"%s\",%d,%d,%d,%d,%lu,%d,%d,?);";

const char select_state[] =
	"SELECT * FROM State ORDER BY date DESC;";

static int
db_open (struct db_info *entry)
{
	int status;
	if (entry == NULL || entry->file_loc == NULL)
		return -1;

	log_info ("Opening database %s", entry->file_loc);

	status = sqlite3_open (entry->file_loc, &entry->db);
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

	if (!entry->id || pgame->score == 0)
		return 0;

	log_info ("Trying to insert scores to database");

	if (db_open (entry) < 0)
		return -1;

	/* Make sure the db has the proper tables */
	sqlite3_prepare_v2 (entry->db, create_scores,
			sizeof create_scores, &stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	ret = asprintf (&insert, insert_scores,
		entry->id, pgame->level, pgame->score, (uint64_t)time(NULL));

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

	debug ("Saving game spaces");

	ret = asprintf (&insert, insert_state,
		entry->id, pgame->score, pgame->lines_destroyed,
		pgame->level, pgame->mod, (uint64_t) time (NULL),
		pgame->width, pgame->height);

	if (ret < 0) {
		log_err ("Out of memory");
	} else {

		sqlite3_prepare_v2 (entry->db, insert, strlen (insert),
				&stmt, NULL);
		free (insert);

		unsigned char data[BLOCKS_ROWS-2];
		memcpy (&data[0], &pgame->spaces[2],
			(BLOCKS_ROWS-2) * sizeof(*pgame->spaces));

		sqlite3_bind_blob (stmt, 1, data, sizeof data, SQLITE_STATIC);
		sqlite3_step (stmt);
		sqlite3_finalize (stmt);
	}

	db_close (entry);

	return 1;
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
		strncpy (entry->id, (const char *)
			sqlite3_column_text (stmt, 0), sizeof entry->id);

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

	/* TODO
	 * Remove single entry from table 
	 */

	const char drop_state[] = "DROP TABLE State;";
	sqlite3_prepare_v2 (entry->db, drop_state, sizeof drop_state,
			&stmt, NULL);
	sqlite3_step (stmt);
	sqlite3_finalize (stmt);

	db_close (entry);

	return ret;
}

/* Returns a pointer to the first element of a tail queue, of @results length.
 * This list can be iterated over to extract the fields:
 * name, level, score, date.
 *
 * Be sure to call db_clean_scores after this to prevent memory leaks.
 * The user can also do it themselves, but it's much easier to call a function.
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
