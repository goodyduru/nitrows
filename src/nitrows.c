#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "extension.h"
#include "net.h"
#include "server.h"
#include "permessage-deflate.h"

/**
 * This function registers Sec-Websocket-Extensions handlers for different points of processing data from accepting connection to responding with data.
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
void nitrows_register_extension(char *key, bool (*validate_offer)(int,ExtensionParam*),
                           uint16_t (*respond_to_offer)(int,char*),
                           bool (*process_data)(int,Frame*,uint8_t**,uint64_t*),
                           uint64_t (*generate_data)(int,uint8_t*,uint64_t,Frame*),
                           void (*close)(int)
                        ) {
    register_extension(key, validate_offer, respond_to_offer, process_data,
                       generate_data, close);
}

int main(){
    extension_table = NULL;
    nitrows_register_extension("permessage-deflate", pmd_validate_offer,
                                pmd_respond, pmd_process_data, pmd_generate_response, pmd_close);
    int listener_socket = get_listener_socket();
    init_event_loop();
    add_to_event_loop(listener_socket);
    run_event_loop(listener_socket, accept_connection, handle_connection);
}