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

/* Set global compiled-in defaults */
struct config configuration = {
	.hostname = CONF_HOSTNAME,
	.port = CONF_PORT,
	.log_dir = CONF_LOGS,
	.saves_dir = CONF_SAVES,
	.conf_dir = CONF_CONFIG,
};

/* Replaces '~' in a pathname with the user's HOME variable.  */
static int conf_replace_home(char *path, size_t len)
{
	char buf[256], *rbuf;

	if (path == NULL || len <= 0)
		return -1;

	const char *home_dir = getenv("HOME");

	if (home_dir == NULL)
		return -1;

	strncpy(buf, path, sizeof buf);
	buf[sizeof(buf) -1] = '\0';

	/* Look for ~ to replace with home_dir
	 * If '~' does not exist in the string we try to replace HOME with
	 * home_dir. If neither exist we fail.
	 */
	rbuf = strtok(buf, "~");
	/* TODO check for HOME */

	memcpy(path, home_dir, len);
	strncat(path, rbuf, len);

	return 1;
}

static int conf_parse(const char *path)
{
	char *fbuf = NULL;
	size_t len = 0;

	logs_to_file("Trying to parse configuration file: %s", path);

	if (file_into_buf(path, &fbuf, &len) != 1) {
		log_err("File \"%s\" does not exist.", path);
		return -1;
	}

	printf("%s\n", fbuf);

	free(fbuf);

	return 1;
}

/* Set default global variables, create necessary directories,
 * read in user specified configuration files.
 */
int conf_init(const char *path)
{
	/* 0755 */
	mode_t mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

	{
		/* Find home */
		conf_replace_home((char *)configuration.log_dir,
			sizeof configuration.log_dir);

		/* make sure all directories exist */
		try_mkdir_r(configuration.log_dir, mode);

		/* append logs file name */
		strlcat((char *)configuration.log_dir, "logs",
			sizeof configuration.log_dir);
	}

	{
		conf_replace_home((char *)configuration.saves_dir,
			sizeof configuration.saves_dir);

		try_mkdir_r(configuration.saves_dir, mode);

		strlcat((char *)configuration.saves_dir, "saves",
			sizeof configuration.saves_dir);
	}

	{
		conf_replace_home((char *)configuration.conf_dir,
			sizeof configuration.conf_dir);

		strlcat((char *)configuration.conf_dir, "tetris.conf",
			sizeof configuration.conf_dir);
	}

	/* Try to read the default compiled-in path for the configuration file,
	 * but don't fail if we can't find it. The user supplied location will
	 * overwrite any changes here.
	 */
	conf_parse(configuration.conf_dir);

	/* User specified configuration file overwrites the default
	 * configuration file path. This one does fail if it doesn't exist
	 */
	if (path && conf_parse(path) != 1)
		log_err("File \"%s\" does not exist.", path);

	return 1;
}
