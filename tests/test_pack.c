
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

	unsigned int ibuf = 0;
	ibuf |= buf[2] << 24;
	ibuf |= buf[3] << 16;
	ibuf |= buf[4] << 8;
	ibuf |= buf[5];
	printf("long: %u\n", ntohl(ibuf));

	unsigned short sbuf = 0;
	sbuf |= buf[7] << 8;
	sbuf |= buf[8];
	printf("short: %u\n", ntohs(sbuf));

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
