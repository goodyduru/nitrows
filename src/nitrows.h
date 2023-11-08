#ifndef NITROWS_SRC_H
#define NITROWS_SRC_H

#include <stdbool.h>

#include "extension.h"

/**
 * This function registers Sec-Websocket-Extensions handlers for different points of processing data from accepting
 * connection to responding with data.
 *
 * @param key: header key to identify extension e.g permessage-deflate
 * @param accept_offer: Handler for accepting extension parameters
 * @param respond_to_offer: Handler for generating a response to a negotiation
 * offer.
 * @param validate_rsv: Handler for validating a frame's rsv
 * @param process_data: Handler for processing client request.
 * @param close: Handler for closing and releasing resources associated with a
 * client.
 */
void nitrows_register_extension(char *key, bool (*validate_offer)(int, ExtensionParam *),
                                uint16_t (*respond_to_offer)(int, char *),
                                bool (*process_data)(int, Frame *, uint8_t **, uint64_t *),
                                uint64_t (*generate_data)(int, uint8_t *, uint64_t, Frame *), void (*close)(int));

/**
 * This function sets up a function for processing a websocket message.
 *
 * @param handle_message: Handler for a websocket message.  This function must accept the following parameters: An
 * integer which is the WebSocket client key, A string which is the message, and an integer which is the message length.
 */
void nitrows_set_message_handler(bool (*handle_message)(int, uint8_t *, uint64_t));

/**
 * This function sends a websocket message
 *
 * @param key: Key to get a WebSocket Client
 * @param message: Message to be sent.
 * @param length: Message Length
 */
bool nitrows_send_message(int key, uint8_t *message, uint64_t length);

void nitrows_run();
#endif