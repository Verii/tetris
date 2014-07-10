#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"

/* Prints a log message of the form:
 * "[time] [ERR|WARN|INFO] message"
 */
void
debug_log (const char *fmt, ...)
{
	char *msg;
	va_list ap;
	time_t s = time (NULL);
	char date[100];

	strftime (date, sizeof date, "[%F %H:%M]", localtime(&s));

	va_start (ap, fmt);
	vasprintf (&msg, fmt, ap);
	va_end (ap);

	if (msg[strlen(msg)-1] == '\n')
		msg[strlen(msg)-1] = '\0';

	fprintf (stderr, "%s %s\n", date, msg);
	free (msg);
}
