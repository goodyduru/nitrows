/**
 * Websocket extension handling functions
 */
#ifndef NITROWS_SRC_EXTENSION_H
#define NITROWS_SRC_EXTENSION_H

#include <stdbool.h>
#include <stdint.h>

#include "clients.h"

#define EXTENSION_TOKEN_LENGTH 31
#define WAITING_CLIENT_TABLE_SIZE 256

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
static WaitingClient *waiting_clients_table[WAITING_CLIENT_TABLE_SIZE];

typedef struct Extension Extension;
struct Extension {
  char *key;  // header extension token
  // This function will validate extension param. It will return true, if all
  // the values are valid. False, otherwise. It will accept a socket
  // descriptor as the first parameter, an extension param object
  bool (*validate_offer)(int, ExtensionParam *params);

  // This function will respond with an extension value. It will return the
  // length of the values written, otherwise false. It will accept a socket
  // descriptor as the first parameter and an array of size 512.
  uint16_t (*respond_to_offer)(int, char *);

  // This function processes data sent by the client. It returns true if
  // valid, false otherwise. It accepts a socket descriptor, a frame array,
  // length of frame array a pointer to the processed data, a pointer to the
  // length of the processed data.
  bool (*process_data)(int, Frame *, uint8_t **, uint64_t *);

  // Generates data to be sent to client. It accepts a socket descriptor, the
  // raw data to be sent, the length of the raw data, a pointer to the
  // output frame, returns the length of data written.
  uint64_t (*generate_data)(int, uint8_t *, uint64_t, Frame *output_frame);

  // Closes and releases all resources associated with a particular socket
  // descriptor.
  void (*close)(int);
};

static Extension *extension_table;
// I mean come on! It shouldn't be more than 256 extensions.
static int8_t extension_count = 0;

/**
 * This function registers Sec-Websocket-Extensions handlers for different points of processing data from accepting
 * connection to responding with data.
 *
 * @param key: header key to identify extension e.g permessage-deflate
 * @param validate_offer: Handler for validating an extension's parameters
 * @param respond_to_offer: Handler for generating a response to a negotiation
 * offer.
 * @param process_data: Handler for processing client request.
 * @param close: Handler for closing and releasing resources associated with a
 * client.
 */
void register_extension(char *key, bool (*validate_offer)(int, ExtensionParam *),
                        uint16_t (*respond_to_offer)(int, char *),
                        bool (*process_data)(int, Frame *, uint8_t **, uint64_t *),
                        uint64_t (*generate_data)(int, uint8_t *, uint64_t, Frame *), void (*close)(int));

/**
 * Making extension table static means it's not available to other module. This
 * function allows others to get the specified extension referenced by
 * @param index
 */
Extension *get_extension(uint8_t index);

/**
 * Get a waiting client from the waiting client table. If none, add
 * a new one to the table and return the extension headers list
 *
 * @param socketfd Client socket descriptor
 * @return Header Extension list
 */
ExtensionList *get_extension_list(int socketfd);

/**
 * Delete extension list and its containing waiting client from the table.
 *
 * @param client Pointer to client struct
 */
void delete_extension_list(int socketfd);

/**
 * Free extension list and its params
 *
 * @param list Pointer to list
 */
void free_extension_list(ExtensionList *list);

/**
 * Get extension parameters from an extensions list with a key.
 *
 * @param list List of extension tokens
 * @param key Extension token to get params
 * @param create Determines if an extension token is created if not found.
 *
 * @returns params
 */
ExtensionParam *get_extension_params(ExtensionList *list, char *key, bool create);

/**
 * Validate each parameters in the client's extension list
 *
 * @param socketfd Client socket
 * @param list List of extension tokens.
 * @param extension_indices Pointer to array of indices.
 * @param indices_count Number of indices count.
 *
 * @returns validity of all the params of all the extension token in list.
 */
bool validate_extension_list(int socketfd, ExtensionList *list, uint8_t **extension_indices, uint8_t *indices_count);
void print_list(ExtensionList *list);
#endif