#ifndef NITROWS_SRC_CLIENTS_H
#define NITROWS_SRC_CLIENTS_H

#include <stdbool.h>
#include <stdint.h>

#include "./defs.h"

#define NO_FRAME (-1)
#define CONTROL_FRAME 0
#define DATA_FRAME 1
#define CONTROL_FRAME_BUFFER_SIZE 125
#define MAX_FRAME_HEADER_SIZE 9

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

    Opcode type; // Frame type
    uint64_t payload_size; // Payload size for current payload

    // The payload of an incomplete or fragrmented data frame has to be stored
    // somewhere. These last 3 elements handle everything concerning this.
    // These values hold across multiple frames of fragmented data. The buffer
    // array contains the data received so far. The filled_size element
    // represents the amount of data in the buffer. The buffer_size element
    // determines the size of the buffer array. 
    uint64_t filled_size;
    uint64_t buffer_size;
    uint64_t current_fragment_offset; // Where current fragment starts
    uint8_t *buffer;
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
    uint8_t current_header[MAX_FRAME_HEADER_SIZE];

    // Frame mask for the currently processed frame.
    uint8_t mask_size;
    uint8_t mask[4];
    
    // Type of current frame. Can be no frame if no current frame is processed
    char current_frame_type;

    Frame control_frame;
    Frame data_frame;
    
    Frame output_frame;
};

typedef struct Node Node;

/**
 * A list node containing a client. It will be used in the hashtable containing 
 * the clients
 */
struct Node {
    // TODO(goody): Test this with and without this member for performance comparision.
    int client_socketfd;

    Client *client;
    Node *next;
};

// Table containing all the connected clients.
static Node* clients_table[HASHTABLE_SIZE];

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

// For debugging purposes
void print_client(Client *client);

#endif // Included clients.h