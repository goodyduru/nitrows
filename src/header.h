/**
 * Websocket connection header functions.
 */
#ifndef NITROWS_SRC_HEADER_H
#define NITROWS_SRC_HEADER_H

#include <stdbool.h>

#include "extension.h"

#define CLIENT_ERROR 400
#define INCOMPLETE_REQUEST_TABLE_SIZE 128

/**
 * Due to unreliable network, a client might not send a complete connection request header. This table is used to store
 * an incomplete connection request. This should be used sparingly based on the standard header size and current network
 * conditions. The struct and struct table is defined below
 */
typedef struct incomplete_request IncompleteRequest;

struct incomplete_request {
  int socketfd;
  uint16_t buffer_size;
  char buffer[4096];
  IncompleteRequest *next;
};

// Table containing all the currently processed connection requests.
static IncompleteRequest *incomplete_request_table[INCOMPLETE_REQUEST_TABLE_SIZE];

/**
 * Initialize an incomplete request structure and add it to the table
 *
 * @param socketfd  socket descriptor
 * @param buffer Buffer containing the response
 * @param buffer_len Length of buffer
 * @return created client struct
 */
void add_request(int socketfd, char buffer[], int buffer_len);

/**
 * Get an incomplete request structure from the table
 *
 * @param socketfd socket descriptor
 * @return incomplete request struct. Null if not found
 */
IncompleteRequest *get_request(int socketfd);

/**
 * Delete an incomplete request struct from the table
 *
 * @param client Pointer to incomplete request struct
 */
void delete_request(IncompleteRequest *request);

/**
 * Move header to next line and make sure newlines are valid.
 *
 * @param string header string
 *
 * @return Increase in position. -1 if invalid
 */
int16_t move_to_next_line(char *start);

/**
 * Check the validity of the `Upgrade` header in the opening
 * request sent by the client. Send an error response if the check fails
 *
 * @param socketfd Client socket descriptor
 * @param start Pointer to start of Upgrade header value
 *
 * @return Increase in position. -1 if invalid
 */
int16_t is_upgrade_header_valid(int socketfd, char *start);

/**
 * Check the validity of the `Sec-Websocket-Key` header in the
 * opening request sent by the client. In addition, get the header value and
 * check its validity too. Send an error response if any of the checks fail.
 * Return the header value in the `key` parameter if all the checks succeed.
 *
 * @param socketfd Client socket descriptor
 * @param start Pointer to start of header value
 * @param key String buffer that will contain the key header value
 *
 * @return Increase in position. -1 if invalid
 */
int16_t get_sec_websocket_key_value(int socketfd, char *start, uint8_t key[]);

/**
 * Check the validity of the `Sec-Websocket-Version` header in the
 * opening request sent by the client. Send an error response if the check fails
 *
 * @param socketfd Client socket descriptor
 * @param start Pointer to start of header value
 *
 * @return Increase in position. -1 if invalid
 */
int16_t is_version_header_valid(int socketfd, char *start);

/**
 * Check the validity of the `Sec-Websocket-Protocol` header in
 * the opening request sent by the client. This header is optional, so absence
 * of the header is valid. Once there's a value, then it has to have a valid
 * format. Send an error response if the checks fails. We return the first
 * subprotocol and its length in the provided parameters.
 *
 * @param socketfd Client socket descriptor
 * @param start Pointer to start of header value
 * @param subprotocol This will contain the chosen protocol
 * @param subprotocol_len This will contain the pointer to the chosen protocol
 * length
 *
 * @return Increase in position. -1 if invalid
 */
int16_t get_subprotocols(int socketfd, char *start, char subprotocol[], int *subprotocol_len);

/**
 * Check the validity of the `Sec-Websocket-Extensions` header in
 * the opening request sent by the client. This header is optional, so absence
 * of the header is valid. Once there's a value, we parse it and hand it over
 * to the available extensions handlers.
 *
 * @param socketfd Client socket descriptor
 * @param start Pointer to start of header value
 * @param token_list List of extension tokens
 * length
 *
 * @return Increase in position. -1 if invalid
 */
int16_t parse_extensions(int socketfd, char *line, ExtensionList *extension_list);

/**
 * Validate header. Check for the different required and optional headers in
 * the http request.
 *
 * @param buf Request string
 * @param request_length Length of request string
 * @param socketfd Client socket descriptor
 * @param key Contains the Sec-Websocket-Key value. This function will
 * concatenate its value and the GUID and add them to the buffer.
 * @param subprotocol Contains the first protocol from the request's
 * Sec-Websocket-Protocol values. Can be empty
 * @param subprotocol_len Subprotocol string length. 0 if subprotocol is empty
 * @param extension_indices Pointer to the pointer of each index of the
 * extensions needed by the client. It is meant to be modified
 * @param indices_count Pointer to the length of the indices array. It is meant
 * to be modified.
 *
 * @returns bool true if response is successfully sent, false otherwise
 *
 */
bool validate_headers(char buf[], uint16_t request_length, int socketfd, uint8_t key[], char subprotocol[],
                      int subprotocol_len, uint8_t **extension_indices, uint8_t *indices_count);

#endif