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

#include "db.h"
#include "blocks.h"
#include "debug.h"
#include "menu.h"

static void
database_init(void)
{
	char file[256];

	/* use default database location */
	snprintf(file, sizeof file, "%s/.local/share/tetris/saves",
			getenv("HOME"));

	db_init(file);
}

static void
touch_dir(void)
{
	/* TODO make this more robust and safer ... */
	char dir[256];

	snprintf(dir, sizeof dir, "%s/.local", getenv("HOME"));
	mkdir(dir, 0644);

	strncat(dir, "/share", sizeof dir - strlen (dir) -1);
	mkdir(dir, 0644);

	strncat(dir, "/tetris", sizeof dir - strlen (dir) -1);
	mkdir(dir, 0644);
}

static void
redirect_stderr(FILE **err_tofile)
{
	char log_file[256];
	size_t len = snprintf(log_file, sizeof log_file,
		"%s/.local/share/tetris/logs", getenv("HOME"));

	if (len >= sizeof log_file)
		return;

	touch_dir();

	fprintf(stderr, "Redirecting stderr to %s\n", log_file);
	*err_tofile = freopen(log_file, "w", stderr);
}

static void
usage(void)
{
	extern const char *__progname;
	fprintf(stderr, "Copyright (C) 2014 James Smith <james@theta.pw>\n"
		"See the LICENSE file included with the release.\n\n"
		"%s-" VERSION " usage:\n\t"
		"[-h] this help\n"
		, __progname);
	exit(EXIT_FAILURE);
}

int
main(int argc, char **argv)
{
	FILE *err_tofile;
	int ch;

	srand(time (NULL));
	setlocale(LC_ALL, "");

	while ((ch = getopt(argc, argv, "h")) != -1) {
		switch (ch) {
		case 'h':
		default:
			usage();
			/* not reached */
			break;
		}
	}
	argc -= optind;
	argv += optind;

	redirect_stderr(&err_tofile);

	/* wrapper to initialize the default db file location */
	database_init();
	curses_init();
	blocks_init();

	do {
		draw_menu();

		switch (getch()) {
		case '1':
			blocks_main();
			/* Reset game to default state after quiting */
			blocks_init();
			break;
		case '2':
			/* populate game with values from save */
			draw_resume();
			blocks_main();

			/* Reset game to default state after quiting */
			blocks_init();
			break;
		case '3':
			break;
		case '4':
			draw_highscores();
			break;
		case '5':
			draw_settings();
			break;
		case '6':
			goto done;
			/* Not reached */
			break;
		default:
			continue;
		}

	} while (1);

done:
	curses_clean();
	blocks_clean();
	db_clean();

	fclose(err_tofile);
	printf("Thanks for playing Tetris-" VERSION "!\n");

	return EXIT_SUCCESS;
}
