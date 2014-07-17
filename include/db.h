#ifndef DB_H_
#define DB_H_

#include <stdint.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <time.h>

#include "blocks.h"

struct db_info {
	sqlite3 *db;		/* internal handler */
	char *file_loc;		/* database location on filesystem */
	char id[16];		/* ID(username?) of a game save */
};

/* Linked list of top scores */
TAILQ_HEAD (db_results_head, db_results) results_head;
struct db_results {
	char id[16];
	uint32_t score;
	uint8_t level;
	time_t date;
	TAILQ_ENTRY (db_results) entries;
};

/* These functions automatically open the database specified in db_info,
 * they do their thing and then cleanup after themselves.
 */

/* Saves game state to disk. Can be restored at a later time */
int db_save_state (struct db_info *, struct block_game *);
int db_resume_state (struct db_info *, struct block_game *);

/* Save game score to disk when the player loses a game */
int db_save_score (struct db_info *, struct block_game *);

/* Returns a linked list to (n) highscores in the database */
struct db_results *db_get_scores (struct db_info *, int n);

/* Cleanup memory */
#define db_clean_scores() while (results_head.tqh_first) { \
	TAILQ_REMOVE (&results_head, results_head.tqh_first, entries);}

#endif /* DB_H_ */
