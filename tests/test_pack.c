
#include <arpa/inet.h>
#include <stdio.h>
#include <string.h>

#include "pack.h"

#define DATA 256

int main(void)
{
	char buf[128];
	int len;

	len = pack((char*)&buf, sizeof buf, "uduh", DATA, DATA);
	for (int i = 0; i < len; i++)
		printf("%hhx ", buf[i] & 0xFF);
	puts("");

	unsigned int ibuf;
	memcpy(&ibuf, &buf[2], 4);
	printf("long: %u -> %u\n", ibuf, ntohl(ibuf));

	unsigned short sbuf;
	memcpy(&sbuf, &buf[7], 2);
	printf("short: %u -> %u\n", sbuf, ntohs(sbuf));

	puts("");

	/* unsigned character */
	len = pack((char*)&buf, sizeof buf, "uc", 131);
	for (int i = 0; i < len; i++)
		printf("%hhd ", buf[i] & 0xFF);
	puts("");

	/* signed character */
	len = pack((char*)&buf, sizeof buf, "c", 131);
	for (int i = 0; i < len; i++)
		printf("%hhd ", buf[i] & 0xFF);
	puts("");

	return 0;
}
