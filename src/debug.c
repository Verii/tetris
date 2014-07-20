#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "debug.h"

/* Prints a log message of the form:
 * "[time] message"
 */
void
debug_log (const char *fmt, ...)
{
	time_t s = time (NULL);
	char *msg, date[32];

	strftime (date, sizeof date, "[%F %H:%M]", localtime(&s));

	va_list ap;
	va_start (ap, fmt);
	vasprintf (&msg, fmt, ap);
	va_end (ap);

	if (msg[strlen(msg)-1] == '\n')
		msg[strlen(msg)-1] = '\0';

	fprintf (stderr, "%s %s\n", date, msg);
	free (msg);
}
