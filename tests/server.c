#include <stdio.h>
#include <stdlib.h>

#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "net.h"

#define HOST "localhost"
#define PORT "1024"

char msg[] = "Goodbye.\n";

int main (void)
{
	int listener, client;

	listener = net_listen (HOST, PORT);

	struct sockaddr_storage addr;
	socklen_t socklen = sizeof addr;

	client = accept (listener, (struct sockaddr *)&addr, &socklen);

	char *ip;

	ip = net_peer_addr(client);
	printf ("Connection from %s\n", ip);
	free (ip);

	net_sendall (client, msg, sizeof msg);

	close (client);
	close (listener);

	return 0;
}
