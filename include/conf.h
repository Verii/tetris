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

#ifndef CONF_H_
#define CONF_H_

#include <stdbool.h>

struct values {
	char *val;
	size_t len;
};

struct key_bindings {
	/* Choose to ignore keypress or not */
	bool enabled;

	/* Registered key */
	int key;

#if 0
	/* Callback for keypress */
	key_cb_func cb;

	union key_cb_val {
		int val_int;
		void *val_ptr;
	};
#endif
};

struct config {
	struct values
		username,	/* username for matchmaking logins WIP */
		password,	/* password for matchmaking logins WIP */
		hostname,	/* localhost */
		port,		/* 10024 */
		logs_file,	/* ~/.local/share/tetris/logs */
		save_file,	/* ~/.local/share/tetris/saves */
		_conf_file;	/* ~/.config/tetris/tetris.conf */

	struct key_bindings
		/* Movement keys */
		move_drop,
		move_left,
		move_right,
		move_down,
		rotate_left,
		rotate_right,
		hold_key,
		pause_key,
		quit_key,

		toggle_ghosts,
		toggle_wallkicks,
		cycle_gamemodes,
		talk_key;
};

/* Read in the specified path. If path is NULL, read in the compiled in default
 * path.
 */
int conf_init(const char *path);

/* Parse an entire configuration set line by line. */
int conf_parse(const char *str, size_t len);

/* Return structure containing global variables */
struct config *conf_get_globals(void);

/* Same but just return a pointer to a static allocation, so we don't have to
 * free it manually.
 */
struct config *conf_get_globals_s(void);

/* Internal functions, these are called when either a "set" "bind" or "say"
 * commands in encountered in the configuration file.
 */
int conf_command_set(const char *cmd, size_t len);
int conf_command_unset(const char *cmd, size_t len);

int conf_command_bind(const char *cmd, size_t len);
int conf_command_say(const char *cmd, size_t len);

void conf_cleanup(void);

#endif /* CONF_H_ */
