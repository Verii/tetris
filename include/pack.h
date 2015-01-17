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
 * Example:
 *
 * 	char buf[256];
 * 	int len = pack(buf, sizeof buf, "uduchus", long, char, short, string);
 *
 * Will create an array containing those elements, in-order, preceeded by the
 * entire array's length. Each element will be preceeded by an identifier.
 *
 * Multi-byte data(i.e. shorts, longs, etc.) are stored in Network Byte Order!
 *
 * The length of the final array is stored in the lower 7 bits of the first octet. [1, 127]
 * If the first bit of the first octet is set, then the size of the data is
 * unspecified. And it's probably larger than 127 bytes.
 *
 * Fields are preceeded by an octet identifier,
 * All identifiers begin with the binary sequence 0100.
 * The remaining lower 4 bits are set to correspond
 * to different data types as follows:
 *
 * signed char		0000	format char. 'c'
 * unsigned char	0001	format char. 'uc'
 * signed short		0010	format char. 'h'
 * unsigned short	0011	format char. 'uh'
 * signed int		0100	format char. 'd'
 * unsigned int		0101	format char. 'ud'
 * signed long		1000	format char. 'l'	*(unimplemented as of 16-01-2015)
 * unsigned long	1001	format char. 'ul'	*(unimplemented as of 16-01-2015)
 * string		1110	format char. 's'	(signed character array)
 * array		1111	format char. 'us'	(unsigned character array)
 *
 * The string and array types are followed by a 1-octet unsigned value
 * designating the length of the data.
 *
 * The datatype identifier is then followed up with the data.
 * Characters are 1 octet.
 * Shorts are 2 octets. (Network Byte Order/Big Endian)
 * Ints are 4 octets.
 * Longs are 8 octets. (not implemented)
 * Strings and Arrays are variable size [1, 255].
 *
 * (ret) contains the address of the location to store the data, must be
 * atleast (buflen) bytes long.
 */
int pack(char *ret, size_t buflen, const char *fmt, ...);

int unpack(const char *buf, size_t buflen, const char *fmt, ...);

#endif
