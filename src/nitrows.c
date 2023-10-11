#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "extension.h"
#include "net.h"
#include "server.h"

/**
 * This function registers Sec-Websocket-Extensions handlers for different points of processing data from accepting connection to responding with data.
 * 
 * @param key: header key to identify extension e.g permessage-deflate
 * @param parse_offer: Handler for parsing an extension parameters
 * @param respond_to_offer: Handler for generating a response to a negotiation
 * offer.
 * @param validate_rsv: Handler for validating a frame's rsv
 * @param process_data: Handler for processing client request.
 * @param close: Handler for closing and releasing resources associated with a
 * client.
*/
void nitrows_register_extension(char *key, bool (*parse_offer)(int,char*,uint16_t),
                           uint16_t (*respond_to_offer)(int,char*),
                           bool (*validate_rsv)(int,bool,bool,bool),
                           bool (*process_data)(int,char*,int,char**,int*),
                           void (*close)(int)
                        ) {
    register_extension(key, parse_offer, respond_to_offer, validate_rsv,
                        process_data, close);
}

int main(){
    extension_table = NULL;
    int listener_socket = get_listener_socket();
    init_event_loop();
    add_to_event_loop(listener_socket);
    run_event_loop(listener_socket, accept_connection, handle_connection);
}