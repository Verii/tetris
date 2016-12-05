/*
 * Copyright (C) 2014  James Smith <james@apertum.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <sqlite3.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "db.h"
#include "logs.h"
#include "tetris.h"

static int
db_open(tetris* pgame, sqlite3** db_handle)
{
  int status = sqlite3_open(pgame->db_file, db_handle);
  if (status != SQLITE_OK) {
    log_warn("DB cannot be opened. (%d)", status);
    return -1;
  }

  return 1;
}

static void
db_close(sqlite3* db_handle)
{
  if (!db_handle) {
    log_warn("Trying to close un-initialized database.");
    return;
  }

  if (sqlite3_close(db_handle) != SQLITE_OK)
    log_warn("Database closed improperly.");
}

/* Scores: name, level, score, date */
const char create_scores[] =
  "CREATE TABLE Scores(name TEXT,level INT,score INT,date INT);";

const char insert_scores[] = "INSERT INTO Scores VALUES(\"%s\",%d,%d,%lu);";

const char select_scores[] = "SELECT * FROM Scores ORDER BY score DESC;";

/* State: name, score, lines, level, date, spaces */
const char create_state[] =
  "CREATE TABLE State(name TEXT,score INT,lines INT,level INT,"
  "date INT,spaces BLOB);";

const char insert_state[] = "INSERT INTO State VALUES(\"%s\",%d,%d,%d,%lu,?);";

const char select_state[] = "SELECT * FROM State ORDER BY date DESC;";

/* Find the entry we just pulled from the database.
 * There's probably a simpler way than using two SELECT calls, but I'm a total
 * SQL noob, so ... */
const char select_state_rowid[] =
  "SELECT ROWID,date FROM State ORDER BY date DESC;";

/* Remove the entry pulled from the database. This lets us have multiple saves
 * in the database concurently.
 */
const char delete_state_rowid[] = "DELETE FROM State WHERE ROWID = ?;";

int
db_save_score(tetris* pgame)
{
  sqlite3* db_handle;
  sqlite3_stmt* stmt;
  char insert[4096];
  int insert_len = -1;

  debug("Trying to insert scores to database");

  if (db_open(pgame, &db_handle) != 1) {
    log_err("Unable to save score.");
    return -1;
  }

  /* Make sure the db has the proper tables */
  sqlite3_prepare_v2(db_handle, create_scores, sizeof create_scores, &stmt,
                     NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  insert_len = snprintf(insert, sizeof(insert), insert_scores, pgame->id,
                        pgame->level, pgame->score, time(NULL));

  if (insert_len >= (int)sizeof(insert)) {
    log_err("String truncated");
    return -1;
  }

  if (insert_len < 0) {
    log_err("Error processing string");
    return -1;
  }

  sqlite3_prepare_v2(db_handle, insert, insert_len, &stmt, NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  db_close(db_handle);

  return 1;
}

int
db_save_state(tetris* pgame)
{
  sqlite3* db_handle;
  sqlite3_stmt* stmt;

  debug("Saving game state to database");

  if (db_open(pgame, &db_handle) != 1) {
    log_err("Unable to save game.");
    return -1;
  }

  /* Create table if it doesn't exist */
  sqlite3_prepare_v2(db_handle, create_state, sizeof create_state, &stmt, NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  char insert[4096];
  int insert_len = -1;
  insert_len =
    snprintf(insert, sizeof(insert), insert_state, pgame->id, pgame->score,
             pgame->lines_destroyed, pgame->level, time(NULL));

  if (insert_len >= (int)sizeof(insert)) {
    log_err("String truncated");
    return -1;
  }

  if (insert_len < 0) {
    log_err("Error processing request");
    return -1;
  }

  /* Length of game board (minus the two hidden rows above the play field */
  int data_len = (TETRIS_MAX_ROWS - 2) * sizeof(*pgame->spaces);
  uint8_t data[data_len];

  memcpy(&data[0], &pgame->spaces[2], data_len);
  sqlite3_prepare_v2(db_handle, insert, strlen(insert), &stmt, NULL);

  sqlite3_bind_blob(stmt, 1, data, data_len, NULL);
  sqlite3_step(stmt);
  sqlite3_finalize(stmt);

  db_close(db_handle);

  return 1;
}

/* Queries database for newest game state information and copies it to pgame.
 */
int
db_resume_state(tetris* pgame)
{
  sqlite3* db_handle;
  sqlite3_stmt *stmt, *delete;
  const char* blob;
  int ret, rowid;

  debug("Trying to restore saved game");

  if (db_open(pgame, &db_handle) != 1 || db_handle == NULL) {
    log_err("Unable to resume game.");
    return -1;
  }

  /* Look for newest entry in table */
  sqlite3_prepare_v2(db_handle, select_state, sizeof select_state, &stmt, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    if (sqlite3_column_text(stmt, 0))
      strncpy(pgame->id, (const char*)sqlite3_column_text(stmt, 0),
              sizeof pgame->id);
    if (sizeof pgame->id)
      pgame->id[sizeof(pgame->id) - 1] = '\0';

    pgame->score = sqlite3_column_int(stmt, 1);
    pgame->lines_destroyed = sqlite3_column_int(stmt, 2);
    pgame->level = sqlite3_column_int(stmt, 3);

    blob = sqlite3_column_blob(stmt, 5);
    memcpy(&pgame->spaces[2], &blob[0],
           (TETRIS_MAX_ROWS - 2) * sizeof(*pgame->spaces));

    ret = 1;
  } else {
    log_info("No game saves found");
    ret = 0;
  }

  sqlite3_finalize(stmt);

  sqlite3_prepare_v2(db_handle, select_state_rowid, sizeof select_state_rowid,
                     &stmt, NULL);

  if (sqlite3_step(stmt) == SQLITE_ROW) {
    rowid = sqlite3_column_int(stmt, 0);

    /* delete from table */
    sqlite3_prepare_v2(db_handle, delete_state_rowid, sizeof delete_state_rowid,
                       &delete, NULL);

    sqlite3_bind_int(delete, 1, rowid);
    sqlite3_step(delete);
    sqlite3_finalize(delete);
  }

  sqlite3_finalize(stmt);
  db_close(db_handle);

  return ret;
}

/* Returns a pointer to the first element of a tail queue, of @n length.
 * This list can be iterated over to extract the fields:
 * name, level, score, date.
 */
int
db_get_scores(tetris* pgame, tetris** res, size_t n)
{
  sqlite3* db_handle;
  sqlite3_stmt* stmt;

  debug("Getting highscores");

  for (size_t i = 0; i < n; i++)
    res[i] = NULL;

  if (db_open(pgame, &db_handle) != 1 || db_handle == NULL) {
    log_err("Unable to get highscores.");
    return -1;
  }

  if (sqlite3_prepare_v2(db_handle, select_scores, sizeof select_scores, &stmt,
                         NULL) != SQLITE_OK) {
    log_err("Unable to query database.");
    return -1;
  }

  size_t i = 0;
  while (i < n && sqlite3_step(stmt) == SQLITE_ROW) {
    if (sqlite3_column_count(stmt) != 4)
      /* improperly formatted row */
      break;

    tetris* np = NULL;
    if ((np = malloc(sizeof *np)) == NULL) {
      log_warn("Out of memory");
      break;
    }

    if (sqlite3_column_text(stmt, 0) == NULL) {
      free(np);
      continue;
    }

    strncpy(np->id, (const char*)sqlite3_column_text(stmt, 0), sizeof np->id);
    if (sizeof np->id > 0)
      np->id[sizeof np->id - 1] = '\0';

    np->level = sqlite3_column_int(stmt, 1);
    np->score = sqlite3_column_int(stmt, 2);
    np->date = sqlite3_column_int(stmt, 3);

    if (!np->score || !np->date) {
      free(np);
      continue;
    }

    res[i++] = np;
  }

  sqlite3_finalize(stmt);
  db_close(db_handle);

  return 1;
}

void
db_clean_scores(tetris** plist, size_t n)
{
  for (size_t i = 0; i < n; i++)
    free(plist[i]);
}
