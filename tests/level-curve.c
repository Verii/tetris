#include <stdio.h>
#include <math.h>

#define PI 3.141592653589

/* As player level increases, the speed of the game ticks will increase,
 * thus reducing the delay between block updates.
 *
 * The difficulty can be adjusted by changing the coefficient in the function.
 * ---
 * The range of f(x) = arctan(x) is [0, PI/2).
 * Bring range down to [0, 1) with coefficients.
 * Increase range to a whole number in [0, (difficult+1)).
 * We then add 1, to bring the range of the function to [1, (difficulty+2)).
 *
 * Easy:	the game ticks approach 1/2 second as level approaches inf.
 * Normal:	the game ticks approach 1/3 second " .
 * Hard:	the game ticks approach 1/4 second " .
 *
 * Dividing the level by 5 reduces the step in the function,
 * for a slower increase in speed.
 */
int main (void)
{
	int i, j;
	double diff;

	struct difficulty {
		char *name;
		int diff;
	} levels[] = {
		{ "Easy", 0 },
		{ "Normal", 1 },
		{ "Hard", 2 },
	};

	for (i = 0; i < 3; i++) {
		printf ("\n%s: Difficulty = %d\n",
				levels[i].name, levels[i].diff);

		for (j = 0; j < 20; j += 5) {
			diff = 1;
			diff += (2/PI * atan(j/5.0)) * (levels[i].diff+1);

			printf ("level: %d => %g%% speed\t", j, diff*100);
			printf ("(tick = %g seconds)\n", 1/diff);
		}
	}

	return 0;
}
