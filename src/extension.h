/**
 * Websocket extension handling functions
 */
#ifndef INCLUDED_EXTENSION_DOT_H
#define INCLUDED_EXTENSION_DOT_H

#include <stdbool.h>
#include <stdint.h>

#define EXTENSION_TOKEN_LENGTH 31
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
 * A param value can be of any type. This enum defines the supported types.
 * It is liable to change in the future.
*/

typedef enum ValueType ValueType;

enum ValueType { EMPTY, INT, BOOL, STRING };

/**
 * Struct for representing a list of extension params. Each param might have a
 * key or not. If there is no key, it implies that the parent extension has the
 * value e.g permessage-default,foo;exclude. will have permessage-default
 * having a value true with no key. The value can be any type and that is
 * accounted for with the enum and union. It is also possible to receive
 * multiple params for a single key. We delineate the different params
 * belonging to a key by using the is_last member. For example
 * permessage-default;client_max_window_bit;server_max_window_bit,
 * permessage-default will have the server_max_window_bit param is_last's
 * member set to true, while that of client_max_window_bit is_last member set
 * to false.
*/
typedef struct extension_param ExtensionParam;

struct extension_param {
    char key[EXTENSION_TOKEN_LENGTH + 1];
    ValueType value_type;
    union {
        int64_t int_type;
        bool bool_type;
        char string_type[EXTENSION_TOKEN_LENGTH + 1];
    };
    bool is_last;
    ExtensionParam *next;
};

/**
 * Defines an extension list. This list will have a token that identifies the
 * extension to use. The params identifies the options that are set by the
 * client. 
*/
typedef struct extension_list ExtensionList;

struct extension_list {
    char token[EXTENSION_TOKEN_LENGTH + 1];
    ExtensionList *next;
    ExtensionParam *params;
};

typedef struct waiting_clients WaitingClient;

struct waiting_clients {
    int socketfd;
    WaitingClient *next;
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
 * 
 * @returns params
*/
ExtensionParam* get_extension_params(ExtensionList *list, char *key, bool create);

void print_list(ExtensionList *list);
#endif