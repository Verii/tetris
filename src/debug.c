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
		/* Give the user control of formatting, but only if they want
		 * more than 1 newline.
		 *
		 * We print a newline by default, so if the user gives one then
		 * we ignore it.
		 *
		 * So that:
		 *      debug_log("blah");
		 * prints the same as
		 *      debug_log("blah\n");
		 *
		 * Yet, 
		 *      debug_log("blah\n\n");
		 * works as expected.
		 */
		int len = strlen(msg);
		if (msg[len] == '\n')
			msg[len] = '\0';
	}

	fprintf(stderr, "%s %s\n", date, msg ? msg : "");
	free(msg);
}
