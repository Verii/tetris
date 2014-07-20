#include <ctype.h>
#include <ncurses.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "blocks.h"
#include "db.h"
#include "debug.h"
#include "screen.h"

#ifndef DB_FILE
#define DB_FILE "/saves"
#endif

#define BLOCK_CHAR "x"

static const char colors[] = { COLOR_WHITE, COLOR_RED, COLOR_GREEN,
		COLOR_YELLOW, COLOR_BLUE, COLOR_MAGENTA, COLOR_CYAN };

void
screen_init (void)
{
	log_info ("Initializing ncurses context");
	initscr ();

	cbreak ();
	noecho ();
	nonl ();
	keypad (stdscr, TRUE);
	curs_set (0);

	start_color ();
	for (size_t i = 0; i < LEN(colors); i++)
		init_pair (i+1, colors[i], COLOR_BLACK);
}

/* Ask user for difficulty and their name */
void
screen_draw_menu (struct block_game *pgame, struct db_info *psave)
{
	/* TODO: user menu pre-game
	 *
	 * New Game
	 * 	> <Name>
	 * 	> [Difficulty]
	 * 	> (Board size) ...
	 * 	{
	 * 	 <Local>
	 * 	 <Network>
	 * 	   {
	 * 		> Client: <server IP> <server PORT>
	 * 		> Server: <listen IP> <listen PORT>
	 *	   }
	 * 	 }
	 *
	 * Resume
	 * 	> Select Game
	 * 		> ...
	 * Settings
	 * 	> Log File: [file]
	 * 	> Database File: [file]
	 * 	> Block Character: <char>
	 *
	 * Quit
	 */
	memset (psave, 0, sizeof *psave);
	strncpy (psave->id, "Lorem Ipsum", sizeof psave->id);

	int ret = asprintf (&psave->file_loc, "%s/.local/share/tetris%s",
					getenv ("HOME"), DB_FILE);
	if (ret < 0) {
		log_err ("Out of memory");
		exit (2);
	}

	pgame->width = 10;
	pgame->height = (pgame->width*2)+2;

	/* Start the game paused if we can resume from an old save */
	if (db_resume_state (psave, pgame) > 0) {
		pgame->pause = true;
	}
}


void
screen_draw_game (struct block_game *pgame)
{
	const char game_x_offset = 18;
	const char game_y_offset = 1;

	attr_t text, border;
	text = COLOR_PAIR(1);
	border = A_BOLD | COLOR_PAIR(5);

	attrset (text);
	box (stdscr, 0, 0);

	mvprintw (1, 1, "Tetris-" VERSION);
	mvprintw (3, 2, "Level %d", pgame->level);
	mvprintw (4, 2, "Score %d", pgame->score);

	mvprintw (6, 2, "Next   Save");
	mvprintw (7, 3, "           ");
	mvprintw (8, 3, "           ");


	mvprintw (10, 2, "Controls");
	mvprintw (11, 3, "Pause [F1]");
	mvprintw (12, 3, "Quit [F3]");
	mvprintw (14, 3, "Move [asd]");
	mvprintw (15, 3, "Rotate [qe]");
	mvprintw (16, 3, "Save [space]");

	attrset (border);
	mvvline (game_y_offset, game_x_offset,
			(unsigned) '*', pgame->height-1);

	mvvline (game_y_offset, game_x_offset+pgame->width+1,
			(unsigned) '*', pgame->height-1);

	mvhline ((pgame->height-2)+game_y_offset, game_x_offset,
			(unsigned) '*', pgame->width+2);

	for (size_t i = 0; i < LEN(pgame->next->p); i++) {
		char y, x;
		y = pgame->next->p[i].y;
		x = pgame->next->p[i].x;

		attrset (COLOR_PAIR((pgame->next->color
				%sizeof colors) +1) | A_BOLD);
		mvprintw (y+8, x+4, BLOCK_CHAR);

	}

	for (size_t i = 0; pgame->save && i < LEN(pgame->save->p); i++) {
		char y, x;
		y = pgame->save->p[i].y;
		x = pgame->save->p[i].x;

		attrset (COLOR_PAIR((pgame->save->color
				%sizeof colors) +1) | A_BOLD);
		mvprintw (y+8, x+10, BLOCK_CHAR);
	}

	/* Draw the game board, minus the two hidden rows above the game */
	for (int i = 2; i < pgame->height; i++) {
		move ((i-2)+game_y_offset, game_x_offset+1);

		for (int j = 0; j < pgame->width; j++) {
			if (blocks_at_yx (pgame, i, j)) {
				attrset (COLOR_PAIR((pgame->colors[i][j]
						% sizeof colors) +1) | A_BOLD);
				printw (BLOCK_CHAR);
			} else {
				attrset (text);
				printw ((j%2) ? "." : " ");
			}
		}
	}

	/* Overlay the PAUSED text */
	attrset (text | A_BOLD);
	if (pgame->pause) {
		char y_off, x_off;

		/* Center the text horizontally, place the text slightly above
		 * the middle vertically.
		 */
		x_off = ((pgame->width  -6)/2 +1) +game_x_offset;
		y_off = ((pgame->height -2)/2 -2) +game_y_offset;

		mvprintw (y_off, x_off, "PAUSED");
	}

	refresh ();
}

/* Game over screen */
void
screen_draw_over (struct block_game *pgame, struct db_info *psave)
{
	if (!pgame || !psave)
		return;

	log_info ("Game over.");

	clear ();
	attrset (COLOR_PAIR(1));
	box (stdscr, 0, 0);

	mvprintw (1, 1, "Local Leaderboard");
	mvprintw (2, 3, "Rank\tName\t\tLevel\tScore\tDate");
	mvprintw (LINES-2, 1, "Press F1 to quit.");

	/* DB access is probably the slowest operation in this function.
	 * Especially when the DB starts to get large with many saves and many
	 * high scores.
	 *
	 * refresh() the screen before we access the DB to make it feel faster
	 * to the luser
	 */
	if (pgame->loss) {
		refresh ();
		db_save_score (psave, pgame);
	} else {
		db_save_state (psave, pgame);
		return;
	}

	/* Print score board */
	struct db_results *res = db_get_scores (psave, 10);
	while (res) {
		char *date = ctime (&res->date);
		static unsigned char count = 0;

		count ++;
		mvprintw (count+2, 4, "%2d.\t%-16s%-5d\t%-5d\t%.*s", count,
			res->id, res->level, res->score, strlen (date)-1, date);
		res = res->entries.tqe_next;
	}
	refresh ();

	db_clean_scores ();
	free (psave->file_loc);

	while (getch() != KEY_F(1));
}

void
screen_cleanup (void)
{
	log_info ("Cleaning ncurses context");
	clear ();
	endwin ();
}

/* Get user input, redraw game */
void *
screen_main (void *vp)
{
	struct block_game *pgame = vp;

	int ch;
	while ((ch = getch())) {

		enum block_cmd cmd = -1;

		switch (toupper(ch)) {
		case 'A':
			cmd = MOVE_LEFT;
			break;
		case 'D':
			cmd = MOVE_RIGHT;
			break;
		case 'S':
			cmd = MOVE_DOWN;
			break;
		case 'W':
			cmd = MOVE_DROP;
			break;
		case 'Q':
			cmd = ROT_LEFT;
			break;
		case 'E':
			cmd = ROT_RIGHT;
			break;
		case ' ':
			cmd = SAVE_PIECE;
			break;
		case KEY_F(1):
			pgame->pause = !pgame->pause;
			break;
		case KEY_F(3):
			pgame->pause = false;
			pgame->quit = true;
			break;
		}

		blocks_move (pgame, cmd);
	}

	return NULL;
}
