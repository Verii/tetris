/* This work was originally written by James Smith,
 * and is hereby dedicated to the Public Domain.
 */

#ifndef NETWORKING_H_
#define NETWORKING_H_

/* Establish a TCP listener on port */
int net_listen (const char *host, const char *port);

/* Attempt to connect to host on port */
int net_connect (const char *host, const char *port);

/* Get the IP address of the connecting peer, and store it in ip */
char *net_peer_addr (int fd);

/* Send/Recv all data, potentially send/recv'ing multiple packets */
int net_sendall (int fd, const char *buf, size_t len);
int net_recvall (int fd, char *buf, size_t len);

#endif
