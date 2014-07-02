#ifndef DB_H_
#define DB_H_

#include <stdint.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <time.h>

#include "blocks.h"

struct db_info {
	sqlite3 *db; /* internal handler, don't modify */
	char *file_loc; /* database location */
	char *id; /* unique ID of a game save, e.g. username */
};

/* Linked list of top scores */
LIST_HEAD (db_results_head, db_results) results_head;
struct db_results {
	char *id;
	uint32_t score;
	uint8_t level;
	time_t date;
	LIST_ENTRY (db_results) entries;
};

/* Handles creation of database, creation of tables, and adding to the
 * database. It will close the connection, and cleanup.
 */
int db_save_score (struct db_info *, struct block_game *);

/* Saves game state to disk. Can be restored at a later time */
int db_save_state (struct db_info *, struct block_game *);

/* Returns a linked list to @int results in the database */
struct db_results *db_get_scores (struct db_info *, int);
void db_clean_scores (void);

#endif /* DB_H_ */
