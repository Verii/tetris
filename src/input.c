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

#include "conf.h"
#include "tetris.h"
#include "events.h"
#include "input.h"
#include "helpers.h"
#include "logs.h"
#include "screen.h"

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

extern tetris *pgame;
extern struct config *config;

int keyboard_in_handler(events *pev) {
  int ret = -1;

  struct {
    struct key_bindings *key;
    int cmd;
  } actions[] = {
      {&config->move_drop, TETRIS_MOVE_DROP},
      {&config->move_down, TETRIS_MOVE_DOWN},
      {&config->move_left, TETRIS_MOVE_LEFT},
      {&config->move_right, TETRIS_MOVE_RIGHT},
      {&config->rotate_left, TETRIS_ROT_LEFT},
      {&config->rotate_right, TETRIS_ROT_RIGHT},

      {&config->hold_key, TETRIS_HOLD_BLOCK},
      {&config->quit_key, TETRIS_QUIT_GAME},
      {&config->pause_key, TETRIS_PAUSE_GAME},
  };

  char kb_key;
  size_t read_ret;
  read_ret = read(pev->fd, &kb_key, 1);

  if (read_ret <= 0)
    return 0;

  size_t i = 0;
  for (; i < LEN(actions); i++) {
    if (actions[i].key->key != kb_key)
      continue;

    if (!actions[i].key->enabled)
      continue;

    ret = tetris_cmd(pgame, actions[i].cmd);
    screen_update(pgame);
    break;
  }

  if (i >= LEN(actions))
    return 0;

  return ret;
}
