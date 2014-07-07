#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <errno.h>
#include <unistd.h>

#include "debug.h"
#include "blocks.h"
#include "net.h"

void *client_listen (void *vp)
{
	int *fds = vp;
	int fd = fds[0];
	char *buf;
	fd_set listen;

	if ((buf = malloc (1024)) == NULL) {
		log_err ("Out of memory");
		exit (2);
	}

	while (1) {
		FD_ZERO (&listen);
		memset (buf, 0, 1024);

		FD_SET (fd, &listen);

		errno = 0;
		select (fd+1, &listen, NULL, NULL, NULL);

		if (!FD_ISSET(fd, &listen))
			continue;

		read (fd, buf, 1024);
	}

	free (buf);

	return NULL;
}

uint8_t *
serialize (struct block_game *pgame, int *len)
{
	(void) pgame;
	(void) len;

	return NULL;
}

int deserialize (struct block_game *pgame, uint8_t *data, int len)
{
	(void) pgame;
	(void) data;
	(void) len;

	return 1;
}
