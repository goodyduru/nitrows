/**
 * Websocket server functions. We'll handle websocket connection upgrade and
 * data frames here.
 */
#ifndef INCLUDED_SERVER_DOT_H
#define INCLUDED_SERVER_DOT_H

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
 * Send upgrade response handshake to a correct client handshake. The response
 * might include a subprotocol or/and extension header if the request contained
 * them.
 * 
 * @param socketfd Client socket descriptor
 * @param key Contains the Sec-Websocket-Key value. This function will
 * concatenate its value and the GUID and add them to the buffer.
 * @param subprotocol Contains the first protocol from the request's
 * Sec-Websocket-Protocol values. Can be empty
 * @param subprotocol_len Subprotocol string length. 0 if subprotocol is empty
 * @param extension_indices Array of extension index needed by the client
 * @param indices_count Length of the index array
 * 
 * @returns bool true if response is successfully sent, false otherwise
 * 
*/
bool __send_upgrade_response(int socketfd, uint8_t key[], char subprotocol[],
                            int subprotocol_len, uint8_t extension_indices[],
                            uint8_t indices_count);

/**
 * Closes client and delete all info concerning the client
 * 
 * @param client Connected client
*/
void close_client(Client* client);

/**
 * Handles from a client. This is only for clients that are in our
 * connection table.
 * 
 * @param client Connected client
*/
void handle_client_data(Client* client);

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