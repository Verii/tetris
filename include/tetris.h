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

/* Commands */
#define TETRIS_MOVE_LEFT	1
#define TETRIS_MOVE_RIGHT	2
#define TETRIS_MOVE_DOWN	3
#define TETRIS_MOVE_DROP	4
#define TETRIS_ROT_LEFT		5
#define TETRIS_ROT_RIGHT	6
#define TETRIS_HOLD_BLOCK	7
#define TETRIS_SAVE_GAME	8
#define TETRIS_PAUSE_GAME	9
#define TETRIS_GAME_TICK	10

/* Attributes */
#define TETRIS_TOGGLE_GHOSTS	1
#define TETRIS_TOGGLE_WALLKICKS	2
#define TETRIS_TOGGLE_TSPIN	3

typedef struct block block;
typedef struct tetris tetris;

/* Create game state */
int tetris_init(tetris **);

/* Free memory */
int tetris_cleanup(tetris *);

/* Process key command in ch and modify game */
int tetris_cmd(tetris *, int N);

int tetris_set_attribute(tetris *, int A);
int tetris_get_attribute(tetris *, int A);

/* Game modes, we win when the game mode returns 1 */

int tetris_set_win_condition(tetris *, int (*tetris_win_condition)(tetris *));

/* Classic tetris, game goes on until we lose */
int tetris_classic(tetris *);
int tetris_40_lines(tetris *);
int tetris_timed(tetris *);

#endif	/* TETRIS_H_ */
