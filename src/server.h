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
 * Handles connection upgrade using the http protocol and it's successful, 
 * we create a client object and add it to our table. If not, we close the
 * connection and remove it from our loop.
 * 
 * @param socketfd Socket descriptor
 */
void handle_upgrade(int socketfd);

bool __send_upgrade_response(int socketfd, char key[], char subprotocol[],
                            int subprotocol_len, char extension[],
                            int extension_len);

/**
 * Handles data frame from a client. This is only for clients that are in our
 * connection table.
 * 
 * @param client Connected client
*/
void handle_frame(Client* client);
#endif