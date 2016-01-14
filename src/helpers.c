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

const mode_t perm_mode = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

int try_mkdir(const char *path, mode_t mode) {
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
int try_mkdir_r(const char *path, mode_t mode) {
  char pcpy[256];    /* Local copy, strtok modifies first argument */
  char subdir[256];  /* subdirectory we're building */
  char *p, *saveptr; /* next token, save state */

  strncpy(pcpy, path, sizeof pcpy);
  strncpy(subdir, "/", sizeof subdir);

  p = strtok_r(pcpy, "/", &saveptr);

  strncat(subdir, p, sizeof subdir - strlen(subdir) - 1);

  while ((p = strtok_r(NULL, "/", &saveptr)) != NULL) {
    strncat(subdir, "/", sizeof subdir - strlen(subdir) - 1);
    strncat(subdir, p, sizeof subdir - strlen(subdir) - 1);

    if (try_mkdir(subdir, mode) != 1)
      goto err_mkdir_r;
  }

  return 1;

err_mkdir_r:
  log_err("Unable to recursively create directory: %s", path);
  return -1;
}

int file_into_buf(const char *path, char **buf, size_t *len) {
  off_t n = -1;
  int fd = -1;

  debug("Trying to open: \"%s\"", path);

  fd = open(path, O_RDONLY);

  if (fd == -1) {
    log_info("open: %s", path);
    log_info("open: %s", strerror(errno));
    goto cleanup;
  }

  debug("File opened: %d", fd);

  /* Get length of file. Is stat() or lseek() better here?
   * I would guess stat() would query the FS for the size, and lseek
   * steps through one byte at a time until it finds EOF?
   */
  if ((n = lseek(fd, 0, SEEK_END)) == -1) {
    log_err("lseek: %s", strerror(errno));
    goto cleanup;
  }

  debug("File length: %d", n);

  if (n == 0) {
    log_info("File empty.");
    goto cleanup;
  }

  lseek(fd, 0, SEEK_SET);

  *len = n;

  *buf = malloc(n);
  if (*buf == NULL) {
    log_err("malloc: %s", strerror(errno));
    goto cleanup;
  }

  size_t read_ret;
  read_ret = read(fd, *buf, n);

  close(fd);

  if (read_ret <= 0)
    return -1;

  return 1;

cleanup:
  *buf = NULL;
  *len = 0;
  if (fd != -1)
    close(fd);

  return -1;
}

/* Give pack a pointer to a location in the buffer of the next line. */
int getnextline(const char *buf, size_t len, char **pbuf) {
  size_t line_len = 0;
  static size_t offset = 0;

  if (buf == NULL || len == 0 || pbuf == NULL) {
    offset = 0;
    return EOF;
  }

  /* Give back NULL, and return EOF if we run past the buffer */
  if (offset >= len) {
    *pbuf = NULL;
    return EOF;
  }

  /* Find first non-whitespace character */
  while (offset < len && isspace(buf[offset]))
    offset++;

  /* Read in one full line */
  while ((offset + line_len) < len && buf[offset + line_len] != '\n' &&
         buf[offset + line_len] != '\r')
    line_len++;

  *pbuf = calloc(1, line_len + 1);
  if (*pbuf == NULL) {
    log_err("Out of memory");
    return -1;
  }

  /* Take in the line, replace newline or return feed with \0 */
  strncpy(*pbuf, &buf[offset], line_len);
  if (line_len > 0)
    (*pbuf)[line_len] = '\0';

  offset += line_len + 1;

  return offset;
}

/* Replaces '~' in a pathname with the user's HOME variable.  */
int replace_home(char **path, size_t *buf_len) {
  size_t len;
  char *home_dir;
  char *buf;

  if (*path == NULL || *buf_len == 0)
    return -1;

  if ((home_dir = getenv("HOME")) == NULL)
    return -1;

  if ((*path)[0] != '~')
    return 0;

  /* Allocate new buffer large enough to hold modified path */
  len = strlen(home_dir) + *buf_len + 1;
  buf = malloc(len);
  if (buf == NULL) {
    log_err("Out of memory");
    return -1;
  }

  /* Store local copy of path variable */
  strncpy(buf, *path, len);
  if (len > 0)
    buf[len - 1] = '\0';

  /* Extend buffer to hold new path */
  *path = realloc(*path, len);
  *buf_len = len;

  /* Copy path prefix */
  strncpy(*path, home_dir, len);

  /* Copy remainder of path */
  strncat(*path, &buf[1], len - strlen(home_dir) - 1);
  (*path)[len - 1] = '\0';

  free(buf);
  return 1;
}
