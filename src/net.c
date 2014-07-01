/* This work was originally written by James Smith,
 * and is hereby dedicated to the Public Domain.
 */

#include <errno.h>
#include <err.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <netinet/in.h>

#include "net.h"

/* Establish a TCP listener on a port */
int
net_listen (const char *host, const char *port)
{
	int status, listener = 0;

	struct addrinfo *ai, *p;
	struct addrinfo hints = (struct addrinfo ) {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_PASSIVE,
		.ai_socktype = SOCK_STREAM,
	};

	if ((status = getaddrinfo(host, port, &hints, &ai)) < 0) {
		warnx ("getaddrinfo: %s", gai_strerror(status));
		return -1;
	}

	for (p = ai; p; p = p->ai_next) {
		listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (listener < 0) {
			continue;
		}

		int sockopt = 1;
		setsockopt(listener, SOL_SOCKET, SO_REUSEADDR,
				&sockopt, sizeof sockopt);

		if (bind(listener, p->ai_addr, p->ai_addrlen) < 0) {
			close(listener);
			continue;
		}

		break;
	}

	if (!p) {
		warnx ("Failed to bind to %s\n", port);
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);

	errno = 0;
	if (listen(listener, 10) != 0 || errno) {
		warn ("listen: ");
		return -1;
	}

	return listener;
}

int
net_connect (const char *host, const char *port)
{
	int socketfd = -1;
	struct addrinfo *ai, *p;
	struct addrinfo hints = (struct addrinfo) {
		.ai_family = AF_UNSPEC,
		.ai_flags = AI_PASSIVE,
		.ai_socktype = SOCK_STREAM,
	};

	getaddrinfo (host, port, &hints, &ai);

	for (p = ai; p; p = p->ai_next) {
		socketfd = socket (p->ai_family, p->ai_socktype, p->ai_protocol);
		if (socketfd < 0) {
			continue;
		}

		if (connect (socketfd, p->ai_addr, p->ai_addrlen) < 0) {
			close (socketfd);
			continue;
		}

		break;
	}

	if (!p) {
		warnx ("Failed to connect to host %s on %s\n", host, port);
		freeaddrinfo(ai);
		return -1;
	}

	freeaddrinfo(ai);

	return socketfd;
}

/* Make sure that ip is atleast large enough to hold IPv6 addresses
 * The constant INET6_ADDRSTRLEN is defined in netinet/in.h
 */
char *
net_peer_addr (int fd)
{
	struct sockaddr_storage sa;
	socklen_t len = sizeof sa;

	if (fd < 0) {
		return NULL;
	}

	char *ip;
	ip = calloc (1, INET6_ADDRSTRLEN);

	if (!ip) {
		warnx ("realloc: ");
		return NULL;
	}

	errno = 0;
	if (getpeername(fd, (struct sockaddr *) &sa, &len) < 0 || errno) {
		warn ("getpeername: ");
		return NULL;
	}

	errno = 0;
	getnameinfo((const struct sockaddr *) &sa, len,
			ip, INET6_ADDRSTRLEN, NULL, 0, 0);

	if (errno) {
		warn ("getnameinfo: ");
	}

	return ip;
}

int
net_sendall (int fd, const char *buf, size_t len)
{
	uint16_t total;
	int16_t n;

	n = total = 0;
	while (total < len) {
		n = send(fd, buf + total, len - total, 0);
		if (n < 0) {
			break;
		}
		total += n;
	}

	return n < 0 ? -1 : total;
}

int
net_recvall (int fd, char *buf, size_t len)
{
	uint16_t total;
	int16_t n;

	n = total = 0;
	while (total < len) {
		n = recv(fd, buf + total, len - total, 0);
		/* we quit on error (-1) or when no data remains (0) */
		if (n < 1) {
			break;
		}
		total += n;
	}
 
	return n < 0 ? -1 : total;
}
