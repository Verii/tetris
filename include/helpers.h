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

int try_mkdir(const char *path, mode_t mode);

/* Create all subdirectories in a path */
int try_mkdir_r(const char *path, mode_t mode);

/* Read the entire contents of path into buffer */
int file_into_buf(const char *path, char **buf, size_t *len);

#endif /* HELPERS_H_ */
