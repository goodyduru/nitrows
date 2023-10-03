/**
 * Websocket frame handling functions.
 */
#ifndef INCLUDED_FRAME_DOT_H
#define INCLUDED_FRAME_DOT_H

#include "clients.h"

/**
 * Extracts a frame header and sets the necessary client struct members to them
 * 
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 *
 * @returns amount of bytes read
*/
int8_t extract_header_data(Client *client, unsigned char buf[], int size);

/**
 * Get the current frame type and set in client struct.
 * 
 * @param client Connected client
 * @param byte Byte containing FIN, RSVS and OPCODE data
 * 
 * @returns successful set up.
*/
bool get_frame_type(Client *client, unsigned char byte);

/**
 * Checks for the validity of the rsv bits
 * 
 * @param client Connected client
 * @param byte_data Byte containing rsv bits.
 * 
 * @returns validity of rsv bits. False if not valid
*/
bool are_rsv_bits_valid(Client *client, unsigned char byte_data);

/**
 * Get the payload size info and set it in client struct.
 * 
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 * 
 * @returns amount of bytes read
*/
int8_t get_payload_data(Client *client, unsigned char buf[], int size);

/**
 * Handles the various types of control frames
 * 
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 * 
 * @returns amount of bytes read
*/
int8_t handle_control_frame(Client *client, unsigned char buf[], int size);

/**
 * Handles the various types of data frames
 * 
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 * 
 * @returns amount of bytes read
*/
int64_t handle_data_frame(Client *client, unsigned char buf[], int size);

/**
 * Send close frame containing only status code
 * 
 * @param client Connected client
 * @param code Close status code
*/
void send_close_status(Client *client, Status_code code);

/**
 * Send close frame
 * 
 * @param client Connected client
 * @param message Close message
 * @param size Message size
*/
void send_close_frame(Client *client, unsigned char *message, uint8_t size);

/**
 * Send pong frame
 * 
 * @param client Connected client
 * @param message Pong message
 * @param size Message size
 * 
 * @returns true if successful, else false
*/
bool send_pong_frame(Client *client, unsigned char *message, uint8_t size);

/**
 * Send ping frame
 * 
 * @param client Connected client
 * @param message Ping message
 * @param size Message size
 * 
 * @returns true if successful, else false
*/
bool send_ping_frame(Client *client, unsigned char *message, uint8_t size);

/**
 * Send data frame
 * 
 * @param client Connected client
 * @param message Ping message
 * @param size Message size
 * @param is_text Determines if message is binary or text
 * 
 * @returns true if successful, else false
*/
bool send_data_frame(Client *client, unsigned char *message, uint64_t size,
                    bool is_text);
#endif