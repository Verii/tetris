#define _GNU_SOURCE

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Prints a log message of the form:
 * "[time] [ERROR|WARN|INFO] message"
 */
void
debug_log (const char *fmt, ...)
{
	va_list ap;
	time_t t = time (NULL);
	char *msg, *date;

	date = malloc (26);
	if (date) {
		ctime_r (&t, date);
		date[strlen (date)-1] = '\0';
		fprintf (stderr, "[%s] ", date);
		free (date);
	}

	va_start (ap, fmt);
	vasprintf (&msg, fmt, ap);
	va_end (ap);

	fprintf (stderr, "%s\n", msg);
	free (msg);
}
