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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>

#include "conf.h"
#include "logs.h"
#include "helpers.h"

struct config conf;

/* Replaces '~' in a pathname with the user's HOME variable.  */
static int conf_replace_home(char *path, size_t len)
{
	char *home_dir;
	char buf[256], *rbuf;

	if (path == NULL || len <= 0)
		return -1;

	if ((home_dir = getenv("HOME")) == NULL)
		return -1;

	strncpy(buf, path, sizeof buf);
	buf[sizeof(buf) -1] = '\0';

	/* Look for ~ to replace with home_dir
	 * If '~' does not exist in the string we try to replace HOME with
	 * home_dir. If neither exist we return.
	 */
	rbuf = strtok(buf, "~");
	if (rbuf == NULL)
		return 1;

	/* TODO check for HOME */

	memcpy(path, home_dir, len);
	strncat(path, rbuf, len);

	return 1;
}

static int conf_parse(const char *path)
{
	char *fbuf = NULL;
	size_t i, len = 0;

	logs_to_file("Config file: %s", path);

	if (file_into_buf(path, &fbuf, &len) != 1) {
		log_err("File \"%s\" does not exist.", path);
		return -1;
	}

	/* Valid keys in configuration file */
	struct keys {
		char *p;
		size_t len;
		const char *key;
	} tokens[] = {
//		{ conf.user.val,	conf.user.len,		"username"},
//		{ conf.pass.val,	conf.pass.len,		"password"},
		{ conf.hostname.val,	conf.hostname.len,	"hostname"},
		{ conf.port.val,	conf.port.len,		"port"},
		{ conf.logs_loc.val,	conf.logs_loc.len,	"logfile"},
		{ conf.saves_loc.val,	conf.saves_loc.len,	"dbfile"},
		{ NULL, 0, NULL }
	};

	const char *pbuf = NULL;
	while (getnextline(fbuf, len, &pbuf) != EOF && pbuf != NULL) {
		char *key = NULL, *val = NULL, *pstr;

		if (sscanf(pbuf, " %ms = %ms ", &key, &val) != 2)
			goto again;

		logs_to_file("Found key = \"%s\", value = \"%s\"", key, val);

		/* Remove enclosing double quotes, and fail if the value is not
		 * enclosed in double quotes.
		 */
		if (val[0] != '"')
			goto again;
		i = 1;
		while (val[i] != '"' && val[i] != '\0')
			i++;
		val[i] = '\0';

		/* Apply changes to global variables */
		for (i = 0; tokens[i].key; i++) {
			if ((pstr = strstr(tokens[i].key, key))) {
				strncpy(tokens[i].p, &val[1], tokens[i].len);
				break;
			}
		}

		if (tokens[i].key == NULL)
			logs_to_file("Key: \"%s\" is not implemented", key);

	again:
		free(key);
		free(val);
	}

	free(fbuf);

	conf_replace_home(conf.logs_loc.val, conf.logs_loc.len);
	conf_replace_home(conf.saves_loc.val, conf.saves_loc.len);

#ifdef DEBUG
	for (i = 0; tokens[i].p; i++)
		logs_to_file("%s", tokens[i].p);
#endif

	return 1;
}

static int _init_memory(void)
{
#define initMem(LOC, LEN, DEF) do { \
		LOC.len = LEN; \
		LOC.val = malloc(LEN);\
		if (LOC.val == NULL) \
			goto error; \
		strncpy(LOC.val, DEF, LEN); \
		LOC.val[LEN-1] = '\0'; \
	} while (0);

	memset(&conf, 0, sizeof conf);

	initMem(conf.hostname, 256, CONF_HOSTNAME);
	initMem(conf.port, 16, CONF_PORT);
	initMem(conf.logs_loc, 256, CONF_LOGS);
	initMem(conf.saves_loc, 256, CONF_SAVES);
	initMem(conf.conf_loc, 256, CONF_CONFIG);

	return 1;

error:
	free(conf.hostname.val);
	free(conf.port.val);
	free(conf.logs_loc.val);
	free(conf.saves_loc.val);
	free(conf.conf_loc.val);
	return -1;
}

/* Set default global variables, create necessary directories,
 * read in user specified configuration files.
 */
int conf_init(const char *path)
{
	/* 0755 */
	const mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

	if (_init_memory() != 1) {
		log_err("Out of memory");
		return -1;
	}

	/* ~/.local/share/tetris */
	conf_replace_home(conf.logs_loc.val, conf.logs_loc.len);
	try_mkdir_r(conf.logs_loc.val, mode);
	strncat(conf.logs_loc.val, "logs", conf.logs_loc.len);

	/* ~/.local/share/tetris */
	conf_replace_home(conf.saves_loc.val, conf.saves_loc.len);
	try_mkdir_r(conf.saves_loc.val, mode);
	strncat(conf.saves_loc.val, "saves", conf.saves_loc.len);

	/* ~/.config/tetris */
	conf_replace_home(conf.conf_loc.val, conf.conf_loc.len);
	try_mkdir_r(conf.conf_loc.val, mode);
	strncat(conf.conf_loc.val, "tetris.conf", conf.conf_loc.len);

	/* Try to read the default compiled-in path for the configuration file,
	 * but don't fail if we can't find it. The user supplied location will
	 * overwrite any changes here.
	 */
	conf_parse(conf.conf_loc.val);

	/* User specified configuration file overwrites the default
	 * configuration file path. This one does fail if it doesn't exist.
	 */
	if (path && conf_parse(path) != 1)
		return -1;

	return 1;
}
