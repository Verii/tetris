/*
 * Copyright (C) 2014  James Smith <james@apertum.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/* Non-standard BSD extensions */
#include <bsd/string.h>

#include <sys/stat.h>
#include <sys/types.h>

#include <errno.h>
#include <locale.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "screen.h"
#include "log_queue.h"

/* We can exit() at any point and still safely cleanup */
static void cleanup(void)
{
	screen_cleanup();
	blocks_cleanup();
	log_queue_cleanup();

	/* Game separator */
	fprintf(stderr, "--\n");
	fclose(stderr);

	printf("Thanks for playing Blocks-%s!\n", VERSION);
}

static void usage(void)
{

	const char *LICENSE =
"Copyright (C) 2014 James Smith <james@apertum.org>\n\n"
"This program is free software; you can redistribute it and/or modify\n"
"it under the terms of the GNU General Public License as published by\n"
"the Free Software Foundation; either version 2 of the License, or\n"
"(at your option) any later version.\n\n"
"This program is distributed in the hope that it will be useful,\n"
"but WITHOUT ANY WARRANTY; without even the implied warranty of\n"
"MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n"
"GNU General Public License for more details.\n";

	extern const char *__progname;
	fprintf(stderr, "%s\nBuilt on %s at %s\n"
		"%s-%s usage:\n\t"
		"[-u] usage\n\t"
		"[-h host] hostname to connect to\n\t"
		"[-p port] port to connect to\n",
		LICENSE, __DATE__, __TIME__, __progname, VERSION);

	exit(EXIT_FAILURE);
}

static int try_mkdir(const char *path, mode_t mode)
{
	struct stat sb;

	errno = 0;
	if (stat(path, &sb) == 0 && S_ISDIR(sb.st_mode) &&
		(sb.st_mode & mode) == mode)
		return 1;

	/* Try to create directory if it doesn't exist */
	if (errno == ENOENT) {
		if (mkdir(path, mode) == -1) {
			perror(path);
			return -1;
		}
	}

	/* Adjust permissions if dir exists but can't be read */
	if ((sb.st_mode & mode) != mode) {
		if (chmod(path, mode) == -1) {
			perror(path);
			return -1;
		}
	}

	return 1;
}

static void init(void)
{
	LIST_INIT(&entry_head);

	/* Most file systems limit the size of filenames to 255 octets */
	char *home_env, game_dir[256];
	mode_t mode = S_IRUSR | S_IWUSR | S_IXUSR;

	/* Get user HOME environment variable. Fail if we can't trust the
	 * environment, like if the setuid bit is set on the executable.
	 */
	if ((home_env = secure_getenv("HOME")) != NULL) {
		strlcpy(game_dir, home_env, sizeof game_dir);
	} else {
		fprintf(stderr, "Environment variable $HOME does not exist, "
				"or it is not what you think it is.\n");
		exit(EXIT_FAILURE);
	}

	/* HOME/.local/share/tetris */
	char *dirs[] = {
		"/.local",
		"/share",
		"/tetris",
		NULL,
	};

	for (int i = 0; dirs[i]; i++) {
		strlcat(game_dir, dirs[i], sizeof game_dir);
		if (try_mkdir(game_dir, mode) < 0)
			goto err_subdir;
	}

	/* redirect stderr to log file.
	 * open for writing in append mode ~/.local/share/tetris/logs */
	strlcat(game_dir, "/logs", sizeof game_dir);
	if (freopen(game_dir, "a", stderr) == NULL) {
		fprintf(stderr, "Could not open: %s\n", game_dir);
		exit(EXIT_FAILURE);
	}

	srand(time(NULL));

	/* Create game context */
	if (blocks_init() > 0) {
		printf("Game successfully initialized\n");
		printf("Appending logs to file: %s.\n", game_dir);
	} else {
		printf("Failed to initialize game\n");
		exit(EXIT_FAILURE);
	}

	/* create ncurses display */
	screen_init();

	return;

 err_subdir:
	fprintf(stderr, "Unable to create sub directories. Cannot continue\n");
	exit(EXIT_FAILURE);
}

int main(int argc, char **argv)
{
	setlocale(LC_ALL, "");

//	char *host, *port;

	/* Quit if we're not attached to a tty */
	if (!isatty(fileno(stdin)))
		exit(EXIT_FAILURE);

	int ch;
	while ((ch = getopt(argc, argv, "uh:p:")) != -1) {
		switch(ch) {
		case 'h':
			printf("%s\n", optarg);
			break;
		case 'p':
			printf("%s\n", optarg);
			break;
		case 'u':
		default:
			usage();
			break;
		}
	}

	init();
	atexit(cleanup);

	pthread_t input_loop;
	pthread_create(&input_loop, NULL, blocks_input, NULL);

	blocks_loop(NULL);

	/* when blocks_loop returns, kill the input thread and cleanup */
	pthread_cancel(input_loop);

	/* Print scores, tell user they're a loser, etc. */
	screen_draw_over();

	return 0;
}
