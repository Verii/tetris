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
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>

#include "net/pack.h"

int unpacki16(const char *buf, uint16_t *h)
{
	uint16_t tmp;

	tmp = (*buf++) << 8;
	tmp += *buf;

	*h = ntohs(tmp);
	return 2;
}

void packi16(unsigned char *buf, uint16_t h)
{
	*buf++ = h >> 8;
	*buf++ = h & 0xFF;
}

int unpacki32(const char *buf, uint32_t *d)
{
	uint32_t tmp;
	tmp = (*buf++) << 24; tmp += (*buf++) << 16;
	tmp += (*buf++) << 8; tmp += *buf;

	*d = ntohl(tmp);
	return 4;
}

void packi32(unsigned char *buf, uint32_t d)
{
	*buf++ = d >> 24; *buf++ = d >> 16;
	*buf++ = d >> 8; *buf++ = d & 0xFF;
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
	unsigned char buf[256];
	size_t len = 0;
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

		if (*p == '\0')
			break;

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

			if (len +2 > sizeof buf)
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
		case 's':
			s = va_arg(ap, unsigned char *);
			slen = (unsigned char) va_arg(ap, int);
			buf[len++] = (sign ? 14 : 15) + (1 << 6);
			buf[len++] = slen & 0xFF;

			if (!s)
				goto done;

			if (len + slen > sizeof buf)
				goto outofbounds;
			memcpy(&buf[len], s, slen);
			len += slen;
			break;
		default:
			fprintf(stderr, "Error in format string: "
					"%c is not supported\n", *p);
			return 0;
		}

	}

done:
	va_end(ap);

	if (len > sizeof buf || len > buflen)
		goto outofbounds;

	memcpy(ret, &buf[0], len);

	return len;

outofbounds:
	fprintf(stderr, "Data length too large, on char \"%c\"\n", *p);
	va_end(ap);
	return 0;
}

/**
 * Unpacks the buffer (buf) of length (buflen) according to format characters
 * in (fmt).
 *
 * This function conforms to the principle of "strict out, loose in".
 * We do check for signedness, but it's not enforced. Give the user what they want.
 *
 * e.g.
 * If the datatype is of unsigned short, but they ask for short, give them a
 * short. (We do make sure it's actually a short though, and not a long.)
 */
int unpack(const char *buf, size_t buflen, const char *fmt, ...)
{
	size_t len = 0;
	bool sign = true;

	va_list ap;

	/* One of each data type */
	int8_t *c; uint8_t *uc;
	int16_t *h; uint16_t *uh;
	int32_t *d; uint32_t *ud;
	unsigned char **us;
	unsigned char *slen; // length of string or array data

	va_start(ap, fmt);

	const char *p = &fmt[0];
	while (*p != '\0') {
		if (*p == 'u') {
			p++;
			sign = false;
		} else
			sign = true;

		if (*p == '\0')
			break;

		switch (*(p++)) {
		case 'c':
			if (len +2 > buflen)
				goto done;

			if (!(buf[len] == 0x40 || buf[len] == 0x41))
				goto non_conformance;

			len++;
			if (sign) {
				if (!(c = va_arg(ap, int8_t *)))
					goto invalid_pointer;
				*c = buf[len++];
			} else {
				if (!(uc = va_arg(ap, uint8_t *)))
					goto invalid_pointer;
				*uc = buf[len++];
			}
			break;
		case 'h':
			if (len +3 > buflen)
				goto done;

			if (!(buf[len] == 0x42 || buf[len] == 0x43))
				goto non_conformance;

			len++;
			if (sign) {
				if (!(h = va_arg(ap, int16_t *)))
					goto invalid_pointer;
				len += unpacki16(&buf[len], (uint16_t *)h);
			} else {
				if (!(uh = va_arg(ap, uint16_t *)))
					goto invalid_pointer;
				len += unpacki16(&buf[len], uh);
			}
			break;
		case 'd':
			if (len +5 > buflen)
				goto done;

			if (!(buf[len] == 0x44 || buf[len] == 0x45))
				goto non_conformance;

			len++;
			if (sign) {
				if (!(d = va_arg(ap, int32_t *)))
					goto invalid_pointer;
				len += unpacki32(&buf[len], (uint32_t *)d);
			} else {
				if (!(ud = va_arg(ap, uint32_t *)))
					goto invalid_pointer;
				len += unpacki32(&buf[len], ud);
			}
			break;
		case 's':
			/* Enough room for size field? */
			if (len +2 > buflen)
				goto done;

			if (!(buf[len] == 0x4e || buf[len] == 0x4f))
				goto non_conformance;
			len++;

			/* Read the size field, make sure there's enough room
			 * to read string */
			if (len + buf[len] > buflen)
				goto done;

			if (!(us = va_arg(ap, unsigned char **)))
				goto invalid_pointer;
			if (!(slen = va_arg(ap, unsigned char *)))
				goto invalid_pointer;
			if (!(*us = malloc(buf[len] +1)))
				goto out_of_mem;

			*slen = buf[len++];

			memcpy(*us, &buf[len], *slen);
			(*us)[*slen] = '\0';

			len += *slen;
			break;
		default:
			fprintf(stderr, "Error in format string: "
					"%c is not supported\n", *p);
			return 0;
		}
	}
done:

	va_end(ap);
	return len;

non_conformance:
	va_end(ap);
	fprintf(stderr, "There is an error in your data."
		       " 0x%hhx is not what I expected.\nAt format char \'%c\'\n"
		       , buf[len], *p);
	return 0;

out_of_mem:
	va_end(ap);
	fprintf(stderr, "Out of memory\n");
	return 0;

invalid_pointer:
	va_end(ap);
	return 0;
}
