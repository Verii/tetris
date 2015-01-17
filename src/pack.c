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

#include <arpa/inet.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "pack.h"

void packi16(unsigned char *buf, uint16_t h)
{
	*buf++ = h>>8;
	*buf++ = h & 0xFF;
}

void packi32(unsigned char *buf, uint32_t d)
{
	*buf++ = d>>24; *buf++ = d>>16;
	*buf++ = d>>8; *buf++ = d & 0xFF;
}

/**
 * Pack data into a char array, using format (fmt) string to determine the data
 * type.
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
int pack(char *ret, size_t buflen, const char *fmt, ...)
{
	unsigned char buf[255];
	size_t len = 1;
	bool sign = true;

	va_list ap;

	/* One of each data type */
	uint8_t c;
	uint16_t h;
	uint32_t d;
	unsigned char *s;
	unsigned char slen; //length of string or array data

	va_start(ap, fmt);

	const char *p = &fmt[0];
	while (*p != '\0') {
		if (*p == 'u') {
			p++;
			sign = false;
		} else
			sign = true;

		switch (*(p++)) {
		case 'c':
			c = (uint8_t) va_arg(ap, int);
			buf[len++] = (sign ? 0 : 1) + (1 << 6);

			if (len +1 > sizeof buf)
				goto outofbounds;
			buf[len++] = c & 0xFF;
			break;
		case 'h':
			h = (uint16_t) va_arg(ap, int);
			buf[len++] = (sign ? 2 : 3) + (1 << 6);

			if (len +2 > sizeof buf +1)
				goto outofbounds;
			packi16(&buf[len], htons(h));
			len += 2;
			break;
		case 'd':
			d = (uint32_t) va_arg(ap, int);
			buf[len++] = (sign ? 4 : 5) + (1 << 6);

			if (len +4 > sizeof buf)
				goto outofbounds;
			packi32(&buf[len], htonl(d));
			len += 4;
			break;
#if 0
		case 'l':
			l = va_arg(ap, uint64_t);
			buf[len++] = (sign ? 8 : 9) + 1 << 3;
			break;
#endif
		case 's':
			s = (unsigned char *) va_arg(ap, unsigned char *);
			slen = (unsigned char) va_arg(ap, int);
			buf[len++] = (sign ? 14 : 15) + (1 << 6);
			buf[len++] = slen & 0xFF;

			if (len + slen > sizeof buf)
				goto outofbounds;
			memcpy(&buf[len], s, slen);
			len += slen;
			break;
		default:
			fprintf(stderr, "Error in format string: "
					"%c is not supported", *p);
			return -1;
		}

	}

	va_end(ap);

	buf[0] = len & 0xFF;

	if (len >= sizeof buf || len > buflen)
		goto outofbounds;

	memcpy(ret, &buf[0], len);

	return len;

outofbounds:
	fprintf(stderr, "Data length too large, on char \"%c\"\n", *p);
	va_end(ap);
	return -1;
}

