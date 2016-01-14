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

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include <ncurses.h>

#include "screen.h"
#include "db.h"
#include "conf.h"
#include "logs.h"
#include "tetris.h"
#include "helpers.h"

#define PIECES_CHAR WACS_BLOCK

#define PIECES_Y_OFF 4
#define PIECES_X_OFF 3
#define PIECES_HEIGHT 16
#define PIECES_WIDTH 13

#define BOARD_Y_OFF 1
#define BOARD_X_OFF 18
#define BOARD_HEIGHT TETRIS_MAX_ROWS
#define BOARD_WIDTH (TETRIS_MAX_COLUMNS + 2)

#define TEXT_Y_OFF 1
#define TEXT_X_OFF (BOARD_X_OFF + BOARD_WIDTH)
#define TEXT_HEIGHT BOARD_HEIGHT
#define TEXT_WIDTH ((COLS - TEXT_X_OFF) - 1)

static WINDOW *board, *pieces, *text;
const char colors[] = {COLOR_WHITE, COLOR_RED,     COLOR_GREEN, COLOR_YELLOW,
                       COLOR_BLUE,  COLOR_MAGENTA, COLOR_CYAN};

#define SCREEN_COLOR_WHITE 1
#define SCREEN_COLOR_RED 2
#define SCREEN_COLOR_GREEN 3
#define SCREEN_COLOR_YELLOW 4
#define SCREEN_COLOR_BLUE 5
#define SCREEN_COLOR_MAGENTA 6
#define SCREEN_COLOR_CYAN 7

const char *tips[] = {
    "Soft dropping yields 1 point per space",
    "Hard dropping yields 2 points per space",
    "Difficult moves increase points by x1.5",
    "Pausing resets your difficulty bonus",
    "Tetris and T-Spins are difficult moves",
    "You're allowed 1 move during lock timeouts",
    "Move down after dropping to speed up the game",
};

int screen_init(void) {
  debug("Initializing ncurses context");
  initscr();

  cbreak();
  noecho();
  nonl();
  keypad(stdscr, TRUE);
  curs_set(0);

  start_color();
  for (size_t i = 0; i < LEN(colors); i++)
    init_pair(i + 1, colors[i], COLOR_BLACK);

  attrset(COLOR_PAIR(SCREEN_COLOR_WHITE));

  /* Draw static text */
  box(stdscr, 0, 0);
  mvprintw(1, 1, "Tetris-" VERSION);

  mvprintw(PIECES_Y_OFF - 1, PIECES_X_OFF + 1, "Hold   Next");

  pieces = newwin(PIECES_HEIGHT, PIECES_WIDTH, PIECES_Y_OFF, PIECES_X_OFF);

  board = newwin(BOARD_HEIGHT, BOARD_WIDTH, BOARD_Y_OFF, BOARD_X_OFF);

  text = newwin(TEXT_HEIGHT, TEXT_WIDTH, TEXT_Y_OFF, TEXT_X_OFF);

  refresh();

  return 1;
}

void screen_cleanup(void) {
  delwin(board);
  delwin(pieces);
  delwin(text);

  clear();
  endwin();
}

int screen_menu(tetris *pgame) {
  (void)pgame;
  return 1;
}

int screen_update(tetris *pgame) {
  werase(board);
  werase(pieces);
  werase(text);

  size_t i;
  const block *pblock;

  /***************/
  /* draw pieces */
  /***************/

  wattrset(pieces, COLOR_PAIR(SCREEN_COLOR_WHITE));
  box(pieces, 0, 0);

  pblock = HOLD_BLOCK(pgame);

  wattrset(pieces, A_BOLD | COLOR_PAIR((pblock->type % LEN(colors)) + 1));

  for (i = 0; i < LEN(pblock->p); i++)
    mvwadd_wch(pieces, pblock->p[i].y + 2, pblock->p[i].x + 3, PIECES_CHAR);

  pblock = FIRST_NEXT_BLOCK(pgame);

  size_t count = 0;
  while (pblock) {

    wattrset(pieces, A_BOLD | COLOR_PAIR((pblock->type % LEN(colors)) + 1));
    for (i = 0; i < LEN(pblock->p); i++)
      mvwadd_wch(pieces, pblock->p[i].y + 2 + (count * 3), pblock->p[i].x + 9,
                 PIECES_CHAR);

    count++;
    pblock = pblock->entries.le_next;
  }

  /**************/
  /* draw board */
  /**************/

  /* Draw board outline */
  wattrset(board, A_BOLD | COLOR_PAIR(SCREEN_COLOR_BLUE));

  mvwvline(board, 0, 0, '*', TETRIS_MAX_ROWS - 1);
  mvwvline(board, 0, TETRIS_MAX_COLUMNS + 1, '*', TETRIS_MAX_ROWS - 1);

  mvwhline(board, TETRIS_MAX_ROWS - 2, 0, '*', TETRIS_MAX_COLUMNS + 2);

  /* Draw the ghost block */
  pblock = pgame->ghost_block;
  wattrset(board, A_DIM | COLOR_PAIR((pblock->type % LEN(colors)) + 1));

  for (i = 0; i < LEN(pblock->p); i++) {
    mvwadd_wch(board, pblock->p[i].y + pblock->row_off - 2,
               pblock->p[i].x + pblock->col_off + 1, PIECES_CHAR);
  }

  /* Draw the game board, minus the two hidden rows above the game */
  for (i = 2; i < TETRIS_MAX_ROWS; i++) {
    if (pgame->spaces[i] == 0)
      continue;

    size_t j;
    for (j = 0; j < TETRIS_MAX_COLUMNS; j++) {
      if (!tetris_at_yx(pgame, i, j))
        continue;

      wattrset(board,
               A_BOLD | COLOR_PAIR((pgame->colors[i][j] % LEN(colors)) + 1));
      mvwadd_wch(board, i - 2, j + 1, PIECES_CHAR);
    }
  }

  /* Draw the falling block to the board */
  pblock = CURRENT_BLOCK(pgame);
  for (i = 0; i < LEN(pblock->p); i++) {
    wattrset(board, A_BOLD | COLOR_PAIR((pblock->type % LEN(colors)) + 1));

    mvwadd_wch(board, pblock->p[i].y + pblock->row_off - 2,
               pblock->p[i].x + pblock->col_off + 1, PIECES_CHAR);
  }

  /* Draw "PAUSED" text */
  if (pgame->paused) {
    wattrset(board, A_BOLD | COLOR_PAIR(SCREEN_COLOR_WHITE));
    mvwprintw(board, (BOARD_HEIGHT - 1) / 2, (BOARD_WIDTH - 6) / 2, "PAUSED");
  }

  wattrset(board, COLOR_PAIR(SCREEN_COLOR_BLUE));
  mvwprintw(board, BOARD_HEIGHT - 1,
            (BOARD_WIDTH - strlen(pgame->gamemode)) / 2, "%s", pgame->gamemode);

  /******************/
  /* Draw game text */
  /******************/

  wattrset(text, COLOR_PAIR(SCREEN_COLOR_WHITE));
  mvwprintw(text, 1, 2, "Level %7d", tetris_get_level(pgame));
  mvwprintw(text, 2, 2, "Score %7d", tetris_get_score(pgame));
  mvwprintw(text, 3, 2, "Lines %7d", tetris_get_lines(pgame));
  mvwprintw(text, 4, 2, "Difficult %3d", tetris_get_difficult(pgame));
  mvwprintw(text, 5, 2, "%s", pgame->id); // username
#ifdef DEBUG
  mvwprintw(text, 6, 2, "DEBUG");
#endif

  /* Controls */
  {
    extern struct config *config;

    mvwprintw(text, 1, 22, "Pause: %c", config->pause_key.key);
    mvwprintw(text, 2, 22, "Save/Quit: %c", config->quit_key.key);
    mvwprintw(text, 3, 22, "Move: %c%c%c%c", config->move_drop.key,
              config->move_left.key, config->move_down.key,
              config->move_right.key);
    mvwprintw(text, 4, 22, "Rotate: %c%c", config->rotate_left.key,
              config->rotate_right.key);
    mvwprintw(text, 5, 22, "Hold: %c", config->hold_key.key);
  }

  /* Tips */
  wattrset(text, A_UNDERLINE | COLOR_PAIR(SCREEN_COLOR_BLUE));
  mvwprintw(text, 7, 2, "Gameplay Tips");

  static int tip = 0, shows = 0;
  if (shows++ >= 30) {
    tip = rand() % LEN(tips);
    shows = 0;
  }
  wattrset(text, COLOR_PAIR(SCREEN_COLOR_YELLOW));
  mvwprintw(text, 8, 3, "%s", tips[tip]);

  /* Logs and Messages */
  wattrset(text, A_UNDERLINE | COLOR_PAIR(SCREEN_COLOR_MAGENTA));
  mvwprintw(text, 10, 2, "Messages");
  wattrset(text, COLOR_PAIR(SCREEN_COLOR_GREEN));

  struct log_entry *lep, *tmp;
  i = 0;
  int vert_off = 11;

  LIST_FOREACH(lep, &entry_head, entries) {
    /* Display messages, then remove anything that can't fit on
     * screen
     */
    if ((i + vert_off) < TEXT_HEIGHT - 1) {
      mvwprintw(text, vert_off + i, 3, "%.*s", TEXT_WIDTH - 3, lep->msg);
    } else {
      tmp = lep;

      LIST_REMOVE(tmp, entries);
      free(tmp->msg);
      free(tmp);
    }
    i++;
  }

  wnoutrefresh(pieces);
  wnoutrefresh(board);
  wnoutrefresh(text);
  doupdate();

  return 1;
}

/* Game over screen */
int screen_gameover(tetris *pgame) {
  debug("Drawing game over screen");

  clear();
  attrset(COLOR_PAIR(SCREEN_COLOR_WHITE));
  box(stdscr, 0, 0);

  if (LINES < 24) {
    attrset(COLOR_PAIR(SCREEN_COLOR_RED) | A_BOLD);
    mvprintw(1, 1, "Not enough screen to print highscores");
    goto input_wait;
  }

  mvprintw(1, 1, "Local Leaderboard");
#ifdef DEBUG
  printw(" DEBUG");
#endif
  mvprintw(2, 3, "Rank\tName\t\tLevel\t  Score\t\tDate");

  /* Print score board when you lose a game */
  tetris *res[20];

  /* Get LEN top scores from database */
  if (db_get_scores(pgame, res, LEN(res)) != 1) {
    return 0;
  }

  for (size_t i = 0; i < LEN(res) && res[i]; i++) {
    char *date_str = ctime(&((res[i])->date));

    /* Pretty colors for 1st, 2nd, 3rd, and 4th */
    switch (i) {
    case 0:
      attron(COLOR_PAIR(SCREEN_COLOR_BLUE));
      break;
    case 1:
      attron(COLOR_PAIR(SCREEN_COLOR_CYAN));
      break;
    case 2:
      attron(COLOR_PAIR(SCREEN_COLOR_GREEN));
      break;
    case 3:
      attron(COLOR_PAIR(SCREEN_COLOR_YELLOW));
      break;
    case 4:
      attron(COLOR_PAIR(SCREEN_COLOR_RED));
      break;
    default:
      attrset(COLOR_PAIR(SCREEN_COLOR_WHITE));
      break;
    }

    /* Bold the entry we've just added to the highscores */
    bool us;
    if ((tetris_get_score(pgame) == (res[i])->score) &&
        (tetris_get_level(pgame) == (res[i])->level)) {
      attron(A_BOLD);
      us = true;
    } else {
      attroff(A_BOLD);
      us = false;
    }

    mvprintw(i + 3, 2, "%s%2d.\t%-16s%5d\t%7d\t\t%.*s%s", us ? ">>" : "  ",
             i + 1, (res[i])->id ? (res[i])->id : "unknown", (res[i])->level,
             (res[i])->score, strlen(date_str) - 1, date_str, us ? " <<" : "");
  }

  db_clean_scores(res, LEN(res));

input_wait:
  /* Give a 5 second countdown before the user can quit, so they have
   * time to see highscores. And so they don't accidentally quit while
   * furiously pressing buttons when the game suddenly ends.
   */

  /* make getch() non-blocking */
  nodelay(stdscr, true);

  int i = 5;
  while (i >= 0) {
    mvprintw(LINES - 2, 1, "Press any key to continue in [%d] seconds..", i--);
    refresh();
    sleep(1);
    while (getch() != ERR)
      ; // drop all user-input
  }

  /* We need to press a key *after* the timer has finished */
  nodelay(stdscr, false);
  getch();

  return 1;
}
