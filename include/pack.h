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

#ifndef PACK_H_
#define PACK_H_

/**
 * Pack data into a char array, using format (fmt) string to determine the data
 * type.
 *
 * The length of the final array is stored in the first byte. [1, 254]
 *
 * Fields are preceeded by an octet identifier,
 * All identifiers begin with the binary sequence 0100 is the higher order bits
 * of the octet. The remaining lower 4 bits are set to correspond to different
 * data types as follows:
 *
 * signed char		0000 'c'
 * unsigned char	0001 'uc'
 * signed short		0010 'h'
 * unsigned short	0011 'uh'
 * signed int		0100 'd'
 * unsigned int		0101 'ud'
 * signed long		1000 'l'
 * unsigned long	1001 'ul'
 * string		1110 's'	(signed character array)
 * array		1111 'us'	(unsigned character array)
 *
 * The string and array types are followed by a 1-octet unsigned value
 * designating the length of the data.
 *
 * (ret) contains the address of the location to store the data, must be
 * atleast (buflen) bytes long.
 */
int pack(char *ret, size_t buflen, const char *fmt, ...);

#endif
