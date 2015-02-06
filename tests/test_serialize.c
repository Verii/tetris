
#include "tetris.h"
#include "net/serialize.h"

char *data_serial;

int main()
{
	tetris *game;
	tetris_init(&game);

	tetris_set_name(game, "artemis");

	/* Add some blocks to game board */
	tetris_cmd(game, TETRIS_MOVE_DROP);
	tetris_cmd(game, TETRIS_GAME_TICK);

	for (int i = 0; i < 4; i++)
		tetris_cmd(game, TETRIS_MOVE_LEFT);
	tetris_cmd(game, TETRIS_MOVE_DROP);
	tetris_cmd(game, TETRIS_GAME_TICK);

	tetris_cmd(game, TETRIS_MOVE_DROP);
	tetris_cmd(game, TETRIS_GAME_TICK);

	tetris_cmd(game, TETRIS_MOVE_DROP);
	tetris_cmd(game, TETRIS_GAME_TICK);

	tetris_cmd(game, TETRIS_MOVE_DROP);
	tetris_cmd(game, TETRIS_GAME_TICK);

	net_serialize(game, &data_serial);

	printf("%s\n", data_serial);
	free(data_serial);

	tetris_cleanup(game);
}
