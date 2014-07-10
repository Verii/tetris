#ifndef CLIENT_H_
#define CLIENT_H_

#include <stdint.h>

#include "blocks.h"
#include "net.h"

void *client_listen (void *vp);

uint8_t *serialize (struct block_game *, int *n);
int deserialize (struct block_game *, uint8_t *, int n);

#endif /* CLIENT_H_ */
