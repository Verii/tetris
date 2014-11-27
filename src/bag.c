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

#include <string.h>

#include "bag.h"

#define DIRTY_BIT 0x80

static int bag_bag7[NUM_BLOCKS];

/* This is the "Random Generator" algorithm.
 * Create a 'bag' of all seven pieces, then one by one remove an element from
 * the bag. Refill the bag when it's empty.
 *
 * This helps to reduce the length of sequential pieces.
 */
void bag_random_generator(void) {

	uint8_t rng_block;

	/* The order here DOES matter, edit with caution */
	uint8_t avail_blocks[] = {
		O_BLOCK,
		I_BLOCK,
		T_BLOCK,
		L_BLOCK,
		J_BLOCK,
		Z_BLOCK,
		S_BLOCK,
	};

	memset(bag_bag7, DIRTY_BIT, NUM_BLOCKS);

	/*
	 * From the Tetris Guidlines:
	 * 	First piece is never the O, S, or Z blocks.
	 */
	rng_block = rand() % (NUM_BLOCKS - 3) + 1;
	bag_bag7[0] = avail_blocks[rng_block];
	avail_blocks[rng_block] = DIRTY_BIT;

	/* int k = rand() % (NUM_BLOCKS - CHOSEN_ELEMENTS);
	 * k == 1;
	 *
	 * +------+------+------+------+
	 * |      |      |      |      |
	 * | nil  |  T   |  Z   |  S   |
	 * |      |      |      |      |
	 * +------+------+------+------+
	 *           0      1      2
	 *					^
	 *
	 * Z == bag[cur_elm] = avail_blocks[k];
	 *
	 * ---
	 *
	 * int k = rand() % (NUM_BLOCKS - CHOSEN_ELEMENTS);
	 * k == 1;
	 *
	 * +------+------+------+------+
	 * |      |      |      |      |
	 * | nil  |  T   | nil  |  S   |
	 * |      |      |      |      |
	 * +------+------+------+------+
	 *           0             1
	 *   					   ^
	 *
	 * S == bag[cur_elm] = avail_blocks[k];
	 */

	/*
	 * Fill remaining bag locations with available pieces
	 */
	for (uint8_t i = 1; i < NUM_BLOCKS; i++) {

		rng_block = rand() % (NUM_BLOCKS - i) +1;

		size_t get_elm = 0;

		for (; get_elm < NUM_BLOCKS && rng_block; get_elm++)
			if (avail_blocks[get_elm] != DIRTY_BIT)
				rng_block--;

		bag_bag7[i] = avail_blocks[get_elm-1];
		avail_blocks[get_elm-1] = DIRTY_BIT;
	}

	debug("Creating new bag.");
	for (size_t i = 0; i < NUM_BLOCKS; i++)
		debug("block %d, type %d", i, bag_bag7[i]);
}

int bag_next_piece(void) {
	static size_t index = 0;

	int ret = bag_bag7[index];

	/* Mark bag location dirty */
	bag_bag7[index] |= DIRTY_BIT;

	if (++index >= LEN(bag_bag7))
		index = 0;

	return ret;
}

int bag_is_empty(void) {
	for (size_t i = 0; i < LEN(bag_bag7); i++)
		if (bag_bag7[i] < DIRTY_BIT)
			return 0;

	return 1;
}
