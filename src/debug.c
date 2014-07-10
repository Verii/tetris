#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"

/* Prints a log message of the form:
 * "[time] [ERROR|WARN|INFO] message"
 */
void
debug_log (const char *fmt, ...)
{
	char *msg;
	va_list ap;
	time_t s = time (NULL);
	char date[100];

	strftime (date, sizeof date, "%F %H:%M", localtime(&s));

	va_start (ap, fmt);
	vasprintf (&msg, fmt, ap);
	va_end (ap);

	fprintf (stderr, "[%s] %s\n", date, msg);
	free (msg);
}
