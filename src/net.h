/**
 * This module handles networking related features. Accepting new connection
 * and creating a listener socket will be done here.
 */
#ifndef NITROWS_SRC_NET_H
#define NITROWS_SRC_NET_H

/**
 * Create and get our server's listener socket descriptor. This descriptor
 * will be what clients connect to.
 *
 * @returns socket descriptor if successful, otherwise -1.
 */
int get_listener_socket();

/**
 * Accept new connection made to the listener socket desc. If successful, we
 * will add the accepted connection socket descriptor to our event loop.
 *
 * @param listener_socket our own server socket descriptor
 */
void accept_connection(int listener_socket);
#endif