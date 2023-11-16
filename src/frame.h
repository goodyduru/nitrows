/**
 * Websocket frame handling functions.
 */
#ifndef NITROWS_SRC_FRAME_H
#define NITROWS_SRC_FRAME_H

#include "clients.h"

#define MAX_PAYLOAD_VALUE 127

/**
 * Extracts a frame header and sets the necessary client struct members to them
 *
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 *
 * @returns amount of bytes read
 */
int8_t extract_header_data(Client *client, uint8_t buf[], int size);

/**
 * Get the current frame type and set in client struct.
 *
 * @param client Connected client
 * @param byte Byte containing FIN, RSVS and OPCODE data
 *
 * @returns true if first byte of header is valid, otherwise false
 */
bool get_frame_type(Client *client, uint8_t byte);

/**
 * Checks for the validity of the rsv bits
 *
 * @param rsv1
 * @param rsv2
 * @param rsv3
 *
 * @returns validity of rsv bits. False if not valid
 */
bool are_rsv_bits_valid(bool rsv1, bool rsv2, bool rsv3);

/**
 * Get the payload size info and set it in client struct.
 *
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 *
 * @returns amount of bytes read
 */
int8_t get_payload_data(Client *client, const uint8_t buf[], int size);

/**
 * Handles the various types of control frames
 *
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 *
 * @returns amount of bytes read
 */
int8_t handle_control_frame(Client *client, uint8_t buf[], int size);

/**
 * Handles the various types of data frames
 *
 * @param client Connected client
 * @param buf Unprocessed data
 * @param size Size of unprocessed data
 *
 * @returns amount of bytes read
 */
int64_t handle_data_frame(Client *client, uint8_t buf[], int size);

/**
 * Generates a reply code based on the status code.
 *
 * @param status_code Code in close frame
 *
 * @returns status code
 */
int16_t get_reply_code(uint16_t status_code);

/**
 * Send either empty close frame depending on status code
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
void send_close_frame(Client *client, uint8_t *message, uint8_t size);

/**
 * Send pong frame
 *
 * @param client Connected client
 * @param message Pong message
 * @param size Message size
 *
 * @returns true if successful, else false
 */
bool send_pong_frame(Client *client, uint8_t *message, uint8_t size);

/**
 * Send ping frame
 *
 * @param client Connected client
 * @param message Ping message
 * @param size Message size
 *
 * @returns true if successful, else false
 */
bool send_ping_frame(Client *client, uint8_t *message, uint8_t size);

/**
 * Send data frame
 *
 * @param socketfd Socket for the client receiving the message.
 * @param message Data message
 * @param size Size of the message
 *
 * @returns true if successful, else false
 */
bool send_data_frame(int socketfd, uint8_t *message, uint64_t size);

/**
 * Start closing client containing whose socket is set to @param socket.
 *
 * @param socketfd Socket for the client receiving the message.
 *
 * @returns true if successful, else false
 */
void start_closing(int socketfd);
#endif