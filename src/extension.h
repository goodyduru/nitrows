/**
 * Websocket extension handling functions
 */
#ifndef INCLUDED_EXTENSION_DOT_H
#define INCLUDED_EXTENSION_DOT_H

#include <stdbool.h>
#include <stdint.h>

#define EXTENSION_KEY_LENGTH 64
#define EXTENSION_VALUE_LENGTH 256
#define WAITING_CLIENT_TABLE_SIZE 256

typedef struct Extension Extension;
struct Extension {
    char *key; // header extension token
    // This function will parse extension param. It will return true, if all
    // the values are valid. False, otherwise. It will accept a socket
    // descriptor as the first parameter, the header value and the length of
    // the header value.
    bool (*parse_offer)(int,char*,uint16_t);

    // This function will respond with an extension value. It will return the
    // length of the values written, otherwise false. It will accept a socket
    // descriptor as the first parameter, a pointer to a string and a pointer
    // to an integer.
    uint16_t (*respond_to_offer)(int,char*);

    // This function will validate the rsv according to the extension rules.
    // Returns true if valid, false otherwise. It accepts a socket descriptor,
    // rsv1, rsv2 and rsv3.
    bool (*validate_rsv)(int,bool,bool,bool);

    // This function processes data sent by the client. It returns true if
    // valid, false otherwise. It accepts a socket descriptor, the raw data
    // sent, the length of the raw data, a pointer to the processed data, a
    // pointer to the length of the processed data.
    bool (*process_data)(int,char*,int,char**,int*);

    // Generates data to be sent to client. Returns true if valid, false
    // otherwise. It accepts a socket descriptor, the raw data to be sent, the
    // length of the raw data, a pointer to the generated data, a pointer to
    // the length of the generated data.
    bool (*generate_data)(int,char*,int,char**,int*);

    // Closes and releases all resources associated with a particular socket
    // descriptor.
    void (*close)(int);
};

Extension *extension_table;
// I mean come on! It shouldn't be more than 256 extensions.
int8_t extension_count;

/** 
 * This is where a list semi-parsed Sec-Websocket-Extensions header values will 
 * be stored. This struct is made to be flexible i.e the values can be updated.
 */

typedef struct extension_list ExtensionList;

struct extension_list {
    ExtensionList *next;
    char key[EXTENSION_KEY_LENGTH];
    char value[EXTENSION_VALUE_LENGTH];
};

typedef struct waiting_clients WaitingClient;

struct waiting_clients {
    WaitingClient *next;
    int socketfd;
    ExtensionList *extensions;
};

// Table containing all the connected clients.
WaitingClient *waiting_clients_table[WAITING_CLIENT_TABLE_SIZE];

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
void register_extension(char *key, bool (*parse_offer)(int,char*,uint16_t),
                           uint16_t (*respond_to_offer)(int,char*),
                           bool (*validate_rsv)(int,bool,bool,bool),
                           bool (*process_data)(int,char*,int,char**,int*),
                           void (*close)(int)
                        );

/**
 * Get a waiting client from the waiting client table. If none, add 
 * a new one to the table and return the extension headers list
 * 
 * @param socketfd Client socket descriptor
 * @return Header Extension list
 */
 ExtensionList *get_extension_list(int socketfd);

/**
 * Delete extensions and its containing waiting client from the table.
 *
 * @param client Pointer to client struct
 */
void delete_extensions(int socketfd);

/**
 * Get extension parameters from an extensions list with a key. 
 * 
 * @param list List of extension tokens
 * @param key Extension token list
 * @param create Determines if an extension token is created if not found.
*/
char* get_extension_params(ExtensionList *list, char *key, bool create);

void print_list(ExtensionList *list);
#endif