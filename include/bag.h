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

#ifndef BAG_H_
#define BAG_H_

#include <string.h>
#include <stdlib.h>

#include "blocks.h"
#include "debug.h"

/* This is the "Random Generator" algorithm.
 * Create a 'bag' of all seven pieces, then one by one remove an element from
 * the bag. Refill the bag when it's empty.
 *
 * This helps to reduce the length of sequential pieces.
 */
void bag_random_generator(void);
int bag_next_piece(void);
int bag_is_empty(void);

#endif /* BAG_H_ */
