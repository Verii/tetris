#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "net.h"

#define HOST "localhost"
#define PORT "1024"

int main (void)
{
	int server;
	char *ip, *msg;

	server = net_connect (HOST, PORT);

	ip = net_peer_addr (server);
	printf ("Connection from %s\n", ip);
	free (ip);

	msg = malloc (256);

	net_recvall (server, msg, 256);
	printf ("Message from server: %s\n", msg);
	free (msg);

	close (server);

	return 0;
}
