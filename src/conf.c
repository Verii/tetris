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

#include <bsd/string.h>

#include <string.h>
#include <stdlib.h>

#include "conf.h"
#include "logs.h"
#include "helpers.h"

struct config configuration = {
	.hostname = CONF_HOSTNAME,
	.port = CONF_PORT,
	.log_dir = CONF_LOGS,
	.saves_dir = CONF_SAVES,
	.conf_dir = CONF_CONFIG,
};

int conf_parse(const char *path)
{
	logs_to_file("Not implemented: %s", path);
	return 0;
}

int conf_init(const char *path)
{
	char *home_dir;
	mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;

	home_dir = getenv("HOME");

	/* This is ugly, but it works. Kind of. */

	{
		char buf[256], *rbuf;
		strncpy(buf, configuration.log_dir, sizeof buf);
		rbuf = strtok(buf, "~");

		memcpy((char *)configuration.log_dir, home_dir,
				sizeof configuration.log_dir);
		strlcat((char *)configuration.log_dir, rbuf,
				sizeof configuration.log_dir);

		try_mkdir_p(configuration.log_dir, mode);

		strlcat((char *)configuration.log_dir, "logs",
				sizeof configuration.log_dir);
	}


	{
		char buf[256], *rbuf;
		strncpy(buf, configuration.saves_dir, sizeof buf);
		rbuf = strtok(buf, "~");

		memcpy((char *)configuration.saves_dir, home_dir,
				sizeof configuration.saves_dir);
		strlcat((char *)configuration.saves_dir, rbuf,
				sizeof configuration.saves_dir);

		try_mkdir_p(configuration.saves_dir, mode);

		strlcat((char *)configuration.saves_dir, "saves",
				sizeof configuration.saves_dir);
	}


	{
		char buf[256], *rbuf;
		strncpy(buf, configuration.conf_dir, sizeof buf);
		rbuf = strtok(buf, "~");

		memcpy((char *)configuration.conf_dir, home_dir,
				sizeof configuration.conf_dir);
		strlcat((char *)configuration.conf_dir, rbuf,
				sizeof configuration.conf_dir);

		try_mkdir_p(configuration.conf_dir, mode);

		strlcat((char *)configuration.conf_dir, "tetris.conf",
				sizeof configuration.conf_dir);
	}

	/* User specified configuration file overwrite the default
	 * configuration file path. */
	conf_parse(path ? path : configuration.conf_dir);

	/* Variables configuration.log_dir, etc. now contain default values */
	logs_to_file(configuration.hostname);
	logs_to_file(configuration.port);
	logs_to_file(configuration.log_dir);
	logs_to_file(configuration.saves_dir);
	logs_to_file(configuration.conf_dir);

	return 1;
}
