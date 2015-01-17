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

#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include "logs.h"
#include "net/network.h"

static int tcp_sock_fd;
//static int udp_sock_fd;

static void network_cleanup(void)
{
	fprintf(stderr, "Network cleaned up without incident.\n");
	debug("Network Cleanup complete.");
	return;
}

int network_init(const char *host, const char *port)
{
	/* getaddrinfo()
	 * socket()
	 * connect()
	 */

	struct addrinfo *res, *ap;
	struct addrinfo hints = {
		.ai_family = AF_UNSPEC,
		.ai_socktype = SOCK_STREAM,
		.ai_protocol = 0,
		.ai_flags = 0,
		.ai_canonname = NULL,
		.ai_addr = NULL,
		.ai_next = NULL,
	};

	int s = getaddrinfo(host, port, &hints, &res);
	if (s != 0) {
		log_err("getaddrinfo: %s, (%s:%s)", gai_strerror(s),
			host, port);
		return -1;
	}

	for (ap = res; ap; ap = ap->ai_next) {
		tcp_sock_fd = socket(ap->ai_family,
				ap->ai_socktype, ap->ai_protocol);
		if (tcp_sock_fd == -1)
			continue;

		if (connect(tcp_sock_fd, ap->ai_addr, ap->ai_addrlen) == 0)
			break;

		close(tcp_sock_fd);
	}

	if (!ap) {
		freeaddrinfo(res);
		log_err("Could not connect to server");
		return -1;
	}

	freeaddrinfo(res);

	debug("Network Initialization complete.");
	atexit(network_cleanup);

	return tcp_sock_fd;
}
