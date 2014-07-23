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
};

extern struct db_info *psave;

/* Linked list returned to user by db_get_scores() call */
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
int db_save_state (const struct block_game *);
int db_resume_state (struct block_game *);

/* Save game score to disk when the player loses a game */
int db_save_score (const struct block_game *);

/* Returns a linked list to (n) highscores in the database */
struct db_results *db_get_scores (int n);

/* Cleanup memory */
#define db_clean_scores() while (results_head.tqh_first) { \
		struct db_results *tmp = results_head.tqh_first; \
		TAILQ_REMOVE (&results_head, tmp, entries); \
		free (tmp); \
	}

#endif /* DB_H_ */
