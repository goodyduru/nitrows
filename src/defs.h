#ifndef INCLUDED_DEFS_DOT_H
#define INCLUDED_DEFS_DOT_H

/** 
 * Represents the string that is concatenated to the Sec-Websocket-Key received from the client, 
 * before the concatenated string is hashed and encoded with base64 which is then set as
 * Sec-Websocket-Accept header value.
 **/
#define GUID "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"


enum {
    // Default buffer size for receiving data from the network.
    BUFFER_SIZE = 4096, 

    // The only supported version of websocket
    WEBSOCKET_VERSION = 13, 

    // Expected length of the Sec-Websocket-Key sent by the client during handshake
    WEBSOCKET_KEY_LENGTH = 16,

    // Minimum size of a frame header to determine if we start to process the header or not.
    MINIMUM_FRAME_HEADER_SIZE = 5,

    // Maximum payload data that we can handle. This is temp to make dev easier. We'll like this to be configurable
    MAX_PAYLOAD_SIZE = 100 * 1024 * 1024,

    // Array size for hashtable containing all the connected clients
    HASHTABLE_SIZE = 1024
};

// This defines the specified opcode types in a websocket data frame.
enum opcode {
    CONTINUATION = 0, 
    TEXT = 1,
    BINARY = 2,
    // This isn't yet defined by the websocket protocol. We just use this as a threshold to determine if a
    // frame is control or data. Any frame greater than this is control else data.
    THRESHOLD = 7,
    CLOSE = 8,
    PING = 9,
    PONG = 10
};

// This defines the allowed status code used in a websocket close frame.
enum status_code {
    NORMAL = 1000,
    AWAY = 1001,
    PROTOCOL_ERROR = 1002,
    INVALID_TYPE = 1003,
    INVALID_ENCODING = 1007,
    VIOLATION = 1008,
    TOO_LARGE = 1009,
    INVALID_EXTENSION = 1010,
    UNEXPECTED_CONDITION = 1011
};

enum connection_status { CONNECTED = 0, CLOSING, CLOSED };

#endif // Included defs.h