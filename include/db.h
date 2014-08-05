#ifndef DB_H_
#define DB_H_

#include <stdint.h>
#include <sqlite3.h>
#include <sys/queue.h>
#include <time.h>

#include "blocks.h"

/* Linked list returned to user by db_get_scores() call */
TAILQ_HEAD (db_save_head, db_save) save_head;
struct db_save {
	struct block_game save;
	time_t date;
	TAILQ_ENTRY (db_save) entries;
};

int db_init (const char *file);
void db_clean (void);

/* Saves game state to disk. Can be restored at a later time */
int db_save_state (void);
int db_resume_state (void);

/* Save game score to disk when the player loses a game */
int db_save_score (void);

/* Returns a linked list to (n) highscores in the database */
struct db_save *db_get_scores (int n);

/* Cleanup memory */
#define db_clean_scores() while (save_head.tqh_first) { \
		struct db_save *_tmp = save_head.tqh_first; \
		TAILQ_REMOVE (&save_head, _tmp, entries); \
		free (_tmp); \
	}

#endif /* DB_H_ */
