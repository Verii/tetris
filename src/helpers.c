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
#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <stdio.h>
#include <unistd.h>

#include "helpers.h"
#include "logs.h"

int try_mkdir(const char *path, mode_t mode)
{
	struct stat sb;

	errno = 0;
	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode) &&
		(sb.st_mode & mode) == mode)
		return 1;

	/* Try to create directory if it doesn't exist */
	if (errno == ENOENT) {
		if (mkdir(path, mode) == -1) {
			perror(path);
			return -1;
		}
	}

	/* Adjust permissions if dir exists but can't be read */
	if ((sb.st_mode & mode) != mode) {
		if (chmod(path, mode) == -1) {
			perror(path);
			return -1;
		}
	}

	return 1;
}

/* Treated like the POSIX `mkdir -p` switch, create all subdirectories in a
 * path.
 */
int try_mkdir_p(const char *path, mode_t mode)
{
	(void) path;
	(void) mode;
	return 1;

	/* TODO
	 * build the path by parsing the string for '/' characters
	 */

	/* HOME/.local/share/tetris */
	char dir[256], *sub_dirs[] = {
		"/.local",
		"/share",
		"/tetris",
		NULL,
	};

	for (int i = 0; sub_dirs[i]; i++) {
		strlcat(dir, sub_dirs[i], sizeof dir);
		if (try_mkdir(dir, mode) != 1)
			goto err_subdir;
	}

	return 1;

	err_subdir:

	log_err("Cannot create subdirectories in %s", path);
	return -1;
}
