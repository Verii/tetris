#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "net/pack.h"

char buf[128];
int len;

void print_data(void)
{
	printf("Encoded data: (%d bytes)\n", len);
	int i = 0;
	while (i < len)
		printf("%.2hhx ", buf[i++]);
	puts("");

	i = 0;
	while (i < len) {
		switch (buf[i++]) {
		case 0x40:
		case 0x41:
			printf("char byte %d\n", i);
			printf("[c] %.2hhx\n", buf[i]);
			i++;
			break;
		case 0x42:
		case 0x43:
			printf("short byte %d\n", i);
			short *s = (short *) &buf[i];
			printf("[h] %x\n", *s);
			i += 2;
			break;
		case 0x44:
		case 0x45:
			printf("int byte %d\n", i);
			int *d = (int *) &buf[i];
			printf("[d] %x\n", *d);
			i += 4;
			break;
		case 0x48:
		case 0x49:
			break;
		case 0x4e:
		case 0x4f:
			printf("[s] \"%.*s\" ", buf[i], &buf[i+1]);
			i += buf[i] +1;
			break;
		default:
			printf("%.2hhx ", buf[i++]);
			break;
		}
	}
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

	unpack(buf, len, fmt, &ubuf, &sbuf);
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

	unpack(buf, len, fmt, &ubuf, &sbuf);

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

	unpack(buf, len, fmt, &ubuf, &sbuf);

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
	const char *fmt = "us";

	printf("Encoding the string\n\t\"%s\".\n", DATA);

	len = pack((char*)&buf, sizeof buf, fmt, DATA, strlen(DATA) +1);
	print_data();

	char *ret = NULL;
	int ret_len = 0;

	unpack(buf, len, fmt, &ret, &ret_len);

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

	fflush(NULL);

	char *ret;
	int ret_len = 0;

	{
		char false_buf[] = {
			0x43, 0xa0, 0x20, 0x44, 0x01, 0x02, 0x03, 0x04,
			0x4e, 0x1, 65, 65, 65 // pretend a 3 byte string is 1 byte.
		};

		int dummy;
		int len = 0;

		len = unpack(&false_buf[len], sizeof false_buf - len, "h", &dummy);
		len += unpack(&false_buf[len], sizeof false_buf - len, "d", &dummy);
		len += unpack(&false_buf[len], sizeof false_buf - len, "s", &ret, &ret_len);

		printf("%.*s\n", ret_len, ret);

		free(ret);
		ret = NULL;
		ret_len = 0;
	}

	/* Pretend an empty string buffer has 255 bytes. */
	{
		char malicious_buf[] = {
			0x4e, 0xFF,
		};

		printf("%d\n", unpack(malicious_buf, sizeof malicious_buf, "s", &ret, &ret_len));
		free(ret);
	}

	return 0;
}
