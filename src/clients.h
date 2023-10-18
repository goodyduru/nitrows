#ifndef INCLUDED_CLIENTS_DOT_H
#define INCLUDED_CLIENTS_DOT_H

#include <stdbool.h>
#include <stdint.h>

#include "./defs.h"

typedef struct Frame Frame;

/**
 * This struct defines a Websocket frame. The frame references other frames. 
 * A frame can have a type (control or data), it can be a first frame, or a 
 * final frame. The type of frame will be stored in this struct.
 */
struct Frame {
    bool is_first; // Is it a first frame

    // A client can send multiple fragments of data. We need to know if the 
    // currently processed frame is the final frame so that the data can be
    // handled once we receive all its data.
    bool is_final;
    bool rsv1;
    bool rsv2;
    bool rsv3;

    // A client can send a control frame singly or in the middle of a 
    // fragmented data frame. This attribute determines if the frame is a 
    // control frame or not.
    bool is_control;
    Opcode type; // Frame type
    uint64_t payload_size; // Payload size for current payload

    // The payload of an incomplete or fragrmented data frame has to be stored
    // somewhere. These last 3 elements handle everything concerning this.
    // These values hold across multiple frames of fragmented data. The buffer
    // array contains the data received so far. The buffer_size element
    // determines the size of received data in the buffer array. For data
    // frames, the buffer pointer points to a section of the shared buffer
    // array that all data frames share. For control frames, the buffer is just 
    // a buffer of size `payload_size`.
    uint64_t buffer_size;
    unsigned char *buffer;
};

typedef struct Client Client;

/**
 * This struct defines the different attributes of a connected websocket 
 * client. Most of these attributes are concerned with data received from a 
 * client. It is possible for the data to be fragmented into different frames. 
 * It is also possible for a frame to be so large, that our network recv buffer 
 * can't hold it all at once. It is also possible for a client to send a 
 * control frame in the midst of fragmented frames. It is also possible for 
 * different frames to be in our network recv buffer at the same time. This 
 * struct tries to define several attributes that can make it easier for our 
 * code to process these scenarios.
 */
struct Client {
    int socketfd; // client socket descriptor

    // Number of extensions supported for this client
    uint8_t indices_count; 

    // Array of each supported extension index in the extension table.
    uint8_t *extension_indices;

    Connection_status status;

    // It is possible for the header of a frame to be in 2 different network 
    // recv buffers. We need a place to store the header info in this scenario. 
    // The minimum header size is 5, so if a currently processed frame size in 
    // a buffer is less than this number, we store it in the `current_header` 
    // element and record the size in the `header_size` element.
    uint8_t header_size;
    unsigned char current_header[9];

    // Frame mask for the currently processed frame.
    uint8_t mask_size;
    unsigned char mask[4];
    // Current frame that we are handling. Can be a data frame or a control 
    // frame
    Frame *current_frame;

    uint32_t frame_size; // Data frame array size
    uint32_t frame_count; // Number of frames in array
    Frame *data_frames; // Array of frames, starting from the first frame.

    uint64_t buffer_limit; // Buffer size
    uint64_t buffer_size; // Amount of bytes in buffer
    unsigned char *shared_buffer; // Buffer containing all data frame bytes
};

typedef struct Node Node;

/**
 * A list node containing a client. It will be used in the hashtable containing 
 * the clients
 */
struct Node {
    // TODO: Test this with and without this member for performance comparision.
    int client_socketfd;

    Client *client;
    Node *next;
};

// Table containing all the connected clients.
Node *clients_table[HASHTABLE_SIZE];

/**
 * Initialize a websocket client structure and add it to the client table.
 *
 * @param socketfd  Client socket descriptor
 * @param extension_indices Array of extension indexes for reference in the
 * array.
 * @param extension_count Number of extension indexes in the array.
 * @return created client struct
 */
Client *init_client(int socketfd, uint8_t *extension_indices,
                    uint8_t indices_count);  

/**
 * Get a websocket client from the client table.
 * 
 * @param socketfd Client socket descriptor
 * @return client struct. Null if not found
 */
Client *get_client(int socketfd);

/**
 * Delete a client from the client table. Once that's done, free its members 
 * and it.
 *
 * @param client Pointer to client struct
 */
void delete_client(Client *client);

/**
 * Internal function, free client and its members.
 */
void __free_client(Client *client);

// For debugging purposes
void print_client(Client *client);

#endif // Included clients.h