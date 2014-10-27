/*
 * Copyright (C) 2014  James Smith <james@theta.pw>
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

#include "bag.h"

#define FILL_BIT 0x80
#define DIRTY_BIT FILL_BIT

static int bag_bag7[] = { DIRTY_BIT, DIRTY_BIT, DIRTY_BIT,
			  DIRTY_BIT, DIRTY_BIT, DIRTY_BIT, DIRTY_BIT };

/* This is the "Random Generator" algorithm.
 * Create a 'bag' of all seven pieces, then one by one remove an element from
 * the bag. Refill the bag when it's empty.
 *
 * This helps to reduce the length of sequential pieces.
 */
void bag_random_generator(void) {
	uint8_t rng_block, avail_blocks[] = {
		SQUARE_BLOCK,
		LINE_BLOCK,
		T_BLOCK,
		L_BLOCK,
		L_REV_BLOCK,
		Z_BLOCK,
		Z_REV_BLOCK,
	};

	debug("Creating new bag");

	/*
	 * First piece is never the SQUARE, S, or Z blocks.
	 */
retry:
	rng_block = rand() % NUM_BLOCKS;
	if (rng_block == SQUARE_BLOCK ||
	    rng_block == Z_REV_BLOCK ||
	    rng_block == Z_BLOCK)
		goto retry;

	bag_bag7[0] = avail_blocks[rng_block];
	avail_blocks[rng_block] |= FILL_BIT; // Mark dirty


	/*
	 * Fill remaining bag locations with available pieces
	 */
	for (uint8_t i = 1; i < NUM_BLOCKS; i++) {

		/* Keep trying until we find a block that hasn't been used */
		do {
			rng_block = rand() % NUM_BLOCKS;
		} while (avail_blocks[rng_block] >= FILL_BIT);

		bag_bag7[i] = avail_blocks[rng_block];
		avail_blocks[rng_block] |= FILL_BIT;
	}
}

int bag_next_piece(void) {
	static size_t index = 0;
	int tmp = bag_bag7[index];

	/* Mark bag location dirty */
	bag_bag7[index] |= DIRTY_BIT;

	if (++index == NUM_BLOCKS)
		index = 0;

	return tmp;
}

int bag_is_empty(void) {
	for (int i = 0; i < 7; i++)
		if (bag_bag7[i] < DIRTY_BIT)
			return 0;

	return 1;
}
