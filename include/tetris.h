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

/* There will always be ghosts in machines - Asimov */

#ifndef TETRIS_H_
#define TETRIS_H_

#include <ncurses.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdarg.h>
#include <sys/queue.h>
#include <time.h>

/* Check if the given (Y, X) coordinate contains a block */
#define tetris_at_yx(G, Y, X) ((G)->spaces[(Y)] & (1 << (X)))

/* Set/Unset location (Y, X) on board */
#define tetris_set_yx(G, Y, X) ((G)->spaces[(Y)] |= (1 << (X)))
#define tetris_unset_yx(G, Y, X) ((G)->spaces[(Y)] &= ~(1 << (X)))

#define HOLD_BLOCK(G)		((G)->blocks_head.lh_first)
#define CURRENT_BLOCK(G)	(HOLD_BLOCK((G))->entries.le_next)
#define FIRST_NEXT_BLOCK(G)	(CURRENT_BLOCK((G))->entries.le_next)

/* Game dimensions */
#define TETRIS_MAX_COLUMNS	10
#define TETRIS_MAX_ROWS		22

/* Number of "next" blocks */
#define TETRIS_NEXT_BLOCKS_LEN	5

/* Block types */
#define TETRIS_I_BLOCK		1
#define TETRIS_T_BLOCK		2
#define TETRIS_L_BLOCK		3
#define TETRIS_J_BLOCK		4
#define TETRIS_O_BLOCK		5
#define TETRIS_S_BLOCK		6
#define TETRIS_Z_BLOCK		7

#define TETRIS_NUM_BLOCKS	7

typedef struct block block;
struct block {
	uint8_t soft_drop, hard_drop;
	uint8_t type;
	bool hold;	/* Has the block been in the hold box */
	bool t_spin;	/* Did we do a t spin */
	bool lock_delay;/* Have we waited an additional game tick */

	uint8_t col_off, row_off;
	struct pieces {
		int8_t x, y;
	} p[4];

	LIST_ENTRY(block) entries;
};

typedef struct tetris tetris;
struct tetris {
	uint16_t spaces[TETRIS_MAX_ROWS];
	uint16_t level;
	uint32_t lines_destroyed;
	uint32_t score;

	uint8_t *colors[TETRIS_MAX_ROWS];
	uint32_t tick_nsec;		// Nanoseconds between game ticks

	int (*check_win)(tetris *);	// Game over when this return 0

	uint8_t bag[TETRIS_NUM_BLOCKS];	// Get random blocks

	LIST_HEAD(blocks_head, block) blocks_head;
	block *ghost_block;

	/* Attributes */
	bool enable_wallkicks;
	bool enable_tspins;
	bool enable_ghosts;
	bool enable_lock_delay;

	/* State */
	bool paused;
	bool win;
	bool lose;
	bool quit;
	bool difficult; // successive difficult moves
	uint8_t rseed; // random seed

	char gamemode[16];
	char db_file[256];
	char id[16];
	time_t date;
};

/* Create game state */
int tetris_init(tetris **);

/* Free memory */
int tetris_cleanup(tetris *);

/* Commands */
#define TETRIS_MOVE_LEFT	0x00
#define TETRIS_MOVE_RIGHT	0x01
#define TETRIS_MOVE_DOWN	0x02
#define TETRIS_MOVE_DROP	0x03
#define TETRIS_ROT_LEFT		0x04
#define TETRIS_ROT_RIGHT	0x05
#define TETRIS_HOLD_BLOCK	0x06
#define TETRIS_QUIT_GAME	0x07
#define TETRIS_PAUSE_GAME	0x08
#define TETRIS_GAME_TICK	0x09
#define TETRIS_SERIALIZE	0x0A

/* Process key command in ch and modify game */
int tetris_cmd(tetris *, int command);

/* Set Attributes */
#define tetris_set_ghosts(G, B)		((G)->enable_ghosts = (B))
#define tetris_set_wallkicks(G, B)	((G)->enable_wallkicks = (B))
#define tetris_set_tspins(G, B)		((G)->enable_tspins = (B))
#define tetris_set_lockdelay(G, B)	((G)->enable_lock_delay = (B))
int tetris_set_name(tetris *, const char *name);
int tetris_set_dbfile(tetris *, const char *name);

/* Game modes, we win when the game mode returns 1 */
enum TETRIS_GAMES {
	TETRIS_CLASSIC,
	TETRIS_40_LINES,
	TETRIS_TIMED,
	TETRIS_INFINITY,
};
int tetris_set_gamemode(tetris *, enum TETRIS_GAMES);

/* Get Attributes */
enum TETRIS_GAME_STATE {
	TETRIS_WIN,
	TETRIS_LOSE,
	TETRIS_QUIT,
	TETRIS_PAUSED,
};

enum TETRIS_GAME_STATE tetris_get_state(tetris *);
int tetris_get_name(tetris *, char *, size_t);
int tetris_get_dbfile(tetris *, char *, size_t);
#define tetris_get_level(G)		((G)->level)
#define tetris_get_lines(G)		((G)->lines_destroyed)
#define tetris_get_score(G)		((G)->score)
#define tetris_get_delay(G)		((G)->tick_nsec)
#define tetris_get_ghosts(G)		((G)->enable_ghosts)
#define tetris_get_wallkicks(G)		((G)->enable_wallkicks)
#define tetris_get_tspins(G)		((G)->enable_tspins)
#define tetris_get_lockdelay(G)		((G)->enable_lock_delay)
#define tetris_get_difficult(G)		((G)->difficult)

#endif	/* TETRIS_H_ */
