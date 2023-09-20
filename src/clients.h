#ifndef INCLUDED_CLIENTS_DOT_H
#define INCLUDED_CLIENTS_DOT_H

#include <stdbool.h>

#include "./defs.h"

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
    Connection_status status;

    // A client can send multiple fragments of data. We need to know if the 
    // currently processed frame is the final frame so that the data can be
    // handled once we receive all its data.
    bool is_final_frame;

    // A client can send a control frame singly or in the middle of a 
    // fragmented data frame. This attribute determines if the currently 
    // processed frame is a control frame or not. We need to know this to 
    // determine where to store the payload data.
    bool is_control_frame;

    // It is possible for the header of a frame to be in 2 different network 
    // recv buffers. We need a place to store the header info in this scenario. 
    // The minimum header size is 5, so if a currently processed frame size in 
    // a buffer is less than this number, we store it in the `current_header` 
    // element and record the size in the `header_size` element.
    uint8_t header_size;
    char current_header[9];

    // The size of the payload data in the currently processed frame.
    uint64_t payload_size;

    // The amount of payload data added to the control/data buffer for a 
    // currently processed frame.
    uint64_t added_payload_size;

    // Frame mask for the currently processed frame.
    char mask[4];

    // Type of control frame being processed.
    Opcode control_type;

    // This one is an optimization. Most control data will contain at most just 
    // the status code which is 2 bytes. But, some will contain the reason. We 
    // need to be able to handle these without wasting space. If the payload 
    // size is less than/equal to 2, we store it in the control_data element. 
    // Else, we allocate space for the control_extra_data element and store the 
    // rest of the data there.
    char control_data[2];
    char *control_extra_data;

    // Type of data frame being processed. This holds across multiple frames of 
    // fragmented data
    Opcode data_type;

    // The payload of a data frame has to be stored somewhere. These last 3 
    // elements handle everything concerning this. These values hold across 
    // multiple frames of fragmented data. The buffer array contains the data 
    // received so far. The buffer_size element determines the size of received 
    // data in the buffer array. The buffer_limit element determines the max 
    // size of the buffer array. We can increase this size to a limit if it the 
    // array size isn't upto the expected data size.
    uint64_t buffer_size;
    uint64_t buffer_limit;
    char buffer[];
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
 * @return created client struct
 */
Client *init_client(int socketfd);  

/**
 * Get a websocket client from the client table.
 * 
 * @param socketfd Client socket descriptor
 * @return client struct
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

#endif // Included clients.h