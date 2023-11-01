/**
 * Websocket server functions. We'll handle websocket connection upgrade and
 * data frames here.
 */
#ifndef NITROWS_SRC_SERVER_H
#define NITROWS_SRC_SERVER_H

#include "clients.h"
/**
 * A generic function that will handle connection from clients. Socket desc
 * owned by a client in the table will be handled by data frame function,
 * those that are aren't will have their connection upgraded.
 *
 * @param socketfd Socket descriptor
 */
void handle_connection(int socketfd);

/**
 * Send http error response and close the connection
 *
 * @param socketfd Client socket descriptor
 * @param status_code HTTP status code
 * @param message Response body
 */
void send_error_response(int socketfd, int status_code, char message[]);

/**
 * Handles connection upgrade using the http protocol and it's successful,
 * we create a client object and add it to our table. If not, we close the
 * connection and remove it from our loop.
 *
 * @param socketfd Socket descriptor
 */
void handle_upgrade(int socketfd);

/**
 * Closes client and delete all info concerning the client
 *
 * @param client Connected client
 */
void close_client(Client *client);

/**
 * Handles from a client. This is only for clients that are in our
 * connection table.
 *
 * @param client Connected client
 */
void handle_client_data(Client *client);

/**
 * Send a frame to a client.
 *
 * @param client Connected client
 * @param frame Data frame
 * @param size Data frame size
 *
 * @returns true is successful, else false
 */
bool send_frame(Client *client, uint8_t *frame, uint64_t size);
#endif