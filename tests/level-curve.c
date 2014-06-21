#include <stdio.h>
#include <math.h>

#define PI 3.141592653589

/* Get your calculator ready!
 * As player level increases, the speed of the game ticks will increase,
 * thus reducing the delay between block updates.
 *
 * At level 10, playing on Normal, the game will update every 1/2 second.
 *
 * The difficulty can be adjusted by changing the constant in the function.
 *
 * ---
 * The range of f(x) = arctan(x) is [0, PI/2).
 * Bring that down to [0, 1) with constants, then bring it back up to a
 * whole number in { 1, 2, 3} (the difficulty).
 *
 * Dividing the level by 10 reduces the step in the function.
 * Finally, add one to increase the modifier >= 1 so that we don't
 * decrease the speed on the lower levels.
 */

int main (void)
{
	int i, j;
	double diff;

	struct difficulty {
		char *name;
		int mod;
	} levels[] = {
		{ "Easy", 1 },
		{ "Normal", 2 },
		{ "Hard", 3 },
	};

	for (i = 0; i < 3; i++) {
		printf ("\n%s: Difficulty = %d\n",
				levels[i].name, levels[i].mod);

		for (j = 0; j < 10; j += 3) {
			diff = (atan(j/(double)10) * levels[i].mod * 2/PI) +1;
			printf ("level: %d => %g%% speed\n", j, diff*100);
		}
	}

	return 0;
}
