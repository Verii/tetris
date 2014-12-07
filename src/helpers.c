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

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "logs.h"
#include "helpers.h"

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
			log_err("mkdir: %s", strerror(errno));
			return -1;
		}
	}

	/* Adjust permissions if dir exists but can't be read */
	if ((sb.st_mode & mode) != mode) {
		if (chmod(path, mode) == -1) {
			log_err("chmod: %s", strerror(errno));
			return -1;
		}
	}

	return 1;
}

/* Treated like the POSIX `mkdir -p` switch, create all subdirectories in a
 * path.
 */
int try_mkdir_r(const char *path, mode_t mode)
{
	char pcpy[256];		/* Local copy, strtok modifies first argument */
	char subdir[256];	/* subdirectory we're building */
	char *p, *saveptr;	/* next token, save state */

	strncpy(pcpy, path, sizeof pcpy);
	strncpy(subdir, "/", sizeof subdir);

	p = strtok_r(pcpy, "/", &saveptr);

	strncat(subdir, p, sizeof subdir);

	while ((p = strtok_r(NULL, "/", &saveptr)) != NULL) {
		strncat(subdir, "/", sizeof subdir);
		strncat(subdir, p, sizeof subdir);

		if (try_mkdir(subdir, mode) != 1)
			goto err_mkdir_r;
	}

	return 1;

err_mkdir_r:
	log_err("Unable to recursively create directory: %s", path);
	return -1;
}

int file_into_buf(const char *path, char **buf, size_t *len)
{
	off_t n = -1;
	int fd = -1;

	fd = open(path, O_RDONLY);

	if (fd == -1) {
		log_err("open: %s", strerror(errno));
		goto cleanup;
	}

	/* Get length of file. Is stat() or lseek() better here?
	 * I would guess stat() would query the FS for the size, and lseek
	 * steps through one byte at a time until it finds EOF?
	 */
	if ((n = lseek(fd, 0, SEEK_END)) == -1) {
		log_err("lseek: %s", strerror(errno));
		goto cleanup;
	}

	lseek(fd, 0, SEEK_SET);

	*len = n;

	*buf = malloc(n);
	if (*buf == NULL) {
		log_err("malloc: %s", strerror(errno));
		goto cleanup;
	}

	read(fd, *buf, n);
	close(fd);

	return 1;

cleanup:
	if (*buf != NULL)
		free(*buf);

	if (fd != -1)
		close(fd);

	return -1;
}

/* Give pack a pointer to a location in the buffer of the next line. */
int getnextline(const char *buf, size_t len, const char **pbuf)
{
	static size_t offset = 0;

	if (*pbuf == NULL)
		offset = 0;

	if (buf == NULL || len == 0) {
		*pbuf = NULL;
		return EOF;
	}

	/* Give back NULL, and return EOF if we run past the buffer */
	if (offset >= len) {
		*pbuf = NULL;
		return EOF;
	}

	/* Find first whitespace character */
	while (offset < len && !isspace(buf[offset++]))
		;

	/* Find last whitespace character */
	while (offset < len && isspace(buf[offset++]))
		;

	*pbuf = &buf[offset-1];
	return offset;
}
