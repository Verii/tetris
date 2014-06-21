#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/* Prints a log message of the form:
 * "[time] [ERROR|WARN|INFO] message"
 */
void
_debug_log (const char *fmt, ...)
{
	time_t t = time (NULL);
	char *msg, *date;

	date = malloc (26);
	ctime_r (&t, date);
	date[strlen (date)-1] = '\0';

	va_list ap;
	va_start (ap, fmt);
	vasprintf (&msg, fmt, ap);
	va_end (ap);

	fprintf (stderr, "[%s] %s\n", date, msg);

	free (msg);
	free (date);
}
