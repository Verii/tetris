#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"

/* Prints a log message of the form:
 * "[time] message"
 */
void debug_log(const char *fmt, ...)
{
	time_t s = time(NULL);
	char *msg, date[32];
	va_list ap;
	int ret;

	strftime(date, sizeof date, "[%F %H:%M]", localtime(&s));

	va_start(ap, fmt);
	ret = vasprintf(&msg, fmt, ap);
	va_end(ap);

	if (ret < 0) {
		msg = NULL;
	} else {
		int len = strlen(msg);
		if (msg[len] == '\n')
			msg[len] = '\0';
	}

	fprintf(stderr, "%s %s\n", date, msg ? msg : "");
	free(msg);
}
