#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pack.h"

char buf[128];
int len;

void print_data(void)
{
	printf("Encoded data:\n");
	for (int i = 0; i < len; i++)
		printf("%hhx ", buf[i] & 0xFF);
	puts("");
}

int test_char(void)
{
	int DATA = 0x8a;
	const char *fmt = "ucc";

	int ubuf = 0, sbuf = 0;

	printf("Encoding the char %lu as signed and unsigned.\n", DATA);

	len = pack((char*)&buf, sizeof buf, fmt, DATA, DATA);
	print_data();

	unpack(buf, buf[1], fmt, &ubuf, &sbuf);
	if (ubuf != DATA || sbuf != DATA) {
		puts("Char test: failed");
		return 0;
	}

	puts("Char test: passed\n");
	return 1;
}

int test_short(void)
{
	int DATA = 0x0f0a;
	const char *fmt = "uhh";

	int ubuf = 0, sbuf = 0;

	printf("Encoding the short %lu as signed and unsigned.\n", DATA);

	len = pack((char*)&buf, sizeof buf, fmt, DATA, DATA);
	print_data();

	unpack(buf, buf[1], fmt, &ubuf, &sbuf);

	if (ubuf != DATA || sbuf != DATA) {
		puts("Short test: failed");
		return 0;
	}

	puts("Short test: passed\n");
	return 1;
}

int test_long(void)
{
	unsigned int DATA = 0xa9b10f0a;
	const char *fmt = "udd";

	unsigned int ubuf = 0;
	int sbuf = 0;

	printf("Encoding the long %lu as signed and unsigned.\n", DATA);

	len = pack((char*)&buf, sizeof buf, fmt, DATA, DATA);
	print_data();

	unpack(buf, buf[1], fmt, &ubuf, &sbuf);

	if (ubuf != DATA || sbuf != (int)DATA) {
		puts("Long test: failed");

		printf("sbuf = %x\n", sbuf);
		printf("ubuf = %x\n", ubuf);
		return 0;
	}

	puts("Long test: passed\n");
	return 1;
}

int test_string(void)
{
	const char *DATA = "This is a test of strings";
	const char *fmt = "s";

	char *ret = NULL; // This is mandatory! Or unpack() assumes it's a valid pointer.
	int ret_len;

	printf("Encoding the string\n\t\"%s\".\n", DATA);

	len = pack((char*)&buf, sizeof buf, fmt, DATA, strlen(DATA));
	print_data();

	unpack(buf, buf[1], fmt, &ret, &ret_len);

	if (strcmp(ret, DATA) != 0) {
		puts("String test: failed");
		printf("%s", ret);

		return 0;
	}
	free(ret);

	puts("String test: passed\n");
	return 1;
}

int main(void)
{
	if (!test_char() || !test_short() || !test_long() || !test_string())
		return 1;

	return 0;
}
