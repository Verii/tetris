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

/* Define default variables */

#ifndef CONF_HOSTNAME
#define CONF_HOSTNAME "localhost"
#endif

#ifndef CONF_PORT
#define CONF_PORT "10024"
#endif

#ifndef CONF_LOGS
#define CONF_LOGS "~/.local/share/tetris/"
#endif

#ifndef CONF_SAVES
#define CONF_SAVES "~/.local/share/tetris/"
#endif

#ifndef CONF_CONFIG
#define CONF_CONFIG "~/.config/tetris/"
#endif

struct config {
	const char hostname[128];
	const char port[16];

	/* slightly misleading, this is the file. NOT the directory */
	/* Filename is appended to each entry */
	const char log_dir[256]; // "logs"
	const char saves_dir[256]; // "saves"
	const char conf_dir[256]; // "tetris.conf"
};

extern struct config configuration;

/* Read in the specified path. If path is NULL, read in the compiled in default
 * path.
 */
int conf_init(const char *path);

#endif /* CONF_H_ */
