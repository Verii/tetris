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
	va_list ap;
	time_t s = time (NULL);
	char *msg, date[32];

	strftime (date, sizeof date, "[%F %H:%M:%S]", localtime(&s));

	msg = malloc (256 * sizeof(char));
	if (msg) {
		va_start (ap, fmt);
		vsnprintf (msg, 256 * sizeof(char), fmt, ap);
		va_end (ap);

		if (msg[strlen(msg)-1] == '\n')
			msg[strlen(msg)-1] = '\0';

	}

	fprintf (stderr, "%s %s\n", date, msg ? msg :
		"Unable to allocate memory for error message.");

	free (msg);
}
