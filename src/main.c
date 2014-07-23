#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>

#include "blocks.h"
#include "debug.h"
#include "menu.h"

static FILE *err_tofile;

static void
redirect_stderr (void)
{
	struct stat sb;
	char log_file[128];

	snprintf (log_file, sizeof log_file, "%s/.local/share/tetris",
			getenv("HOME"));

	errno = 0;
	stat (log_file, &sb);

	/* segfaults if ~/.local/share doesn not exist */
	if (errno == ENOENT) {
		mkdir (log_file, 0777);
	}

	/* add name of log file to string */
	strcat (log_file, "/logs");

	printf ("Redirecting stderr to %s.\n", log_file);
	err_tofile = freopen (log_file, "w", stderr);
}

static void
usage (void)
{
	extern const char *__progname;
	fprintf (stderr, "Copyright (C) 2014 James Smith <james@theta.pw>\n"
		"See the LICENSE file included with the release\n\n"
		"%s-" VERSION " usage:\n\t"
		"[-h] this help\n"
		, __progname);
}

int
main (int argc, char **argv)
{
	int ch;
	srand (time (NULL));
	setlocale (LC_ALL, "");

	while ((ch = getopt (argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
		default:
			usage();
			exit (EXIT_FAILURE);
		}
	}
	argc -= optind;
	argv += optind;

	redirect_stderr ();

	curses_init ();
	blocks_init ();

	draw_menu ();

	ch = 0;
	do {
		switch (ch) {
		case '1':
			blocks_main ();
			/* reset to defaults if we quit */
			blocks_init ();
			break;
		case '2':
			break;
		case '3':
			draw_highscores ();
			break;
		case '4':
			draw_settings ();
			break;
		case '5':
			goto done;
			/* Not reached */
			break;
		default:
			continue;
		}

		draw_menu ();

	} while ((ch = getch()));

done:
	curses_kill ();
	fclose (err_tofile);

	printf ("Thanks for playing Blocks-" VERSION "!\n");

	return (EXIT_SUCCESS);
}
