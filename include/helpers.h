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

#ifndef HELPERS_H_
#define HELPERS_H_

#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>

#define PI 3.141592653589L
#define LEN(x) ((sizeof(x))/(sizeof(*x)))

/* USER: rwx, GROUP: rwx, OTHER: rx (0755) */
extern const mode_t perm_mode;

/* POSIX `mkdir` command */
int try_mkdir(const char *path, mode_t);

/* Recursively create all subdirectories in a path
 * POSIX `mkdir -p` command */
int try_mkdir_r(const char *path, mode_t);

/* Read the contents of *path into buffer
 * returns buffer in **buf
 * returns length in *len
 */
int file_into_buf(const char *path, char **buf, size_t *len);

/* Find the next line in a buffer of length len, maintain the buffer offset
 * between calls in the offset variable.
 *
 * Returns the next line in the pointer pbuf. This is just a pointer to a
 * location in the buffer. pbuf is NULL if we run past the end of the buffer,
 *
 * Find beginning of a line that isn't whitespace(newline, space, tab, etc.)
 */
int getnextline(const char *buf, size_t len, char **pbuf);

/* Replace '~' and "HOME" with the user's HOME environment variable.
 */
int replace_home(char **, size_t *len);

#endif /* HELPERS_H_ */
