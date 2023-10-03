#include <stdio.h>
#include <stdlib.h>

#include "./clients.h"

Client *init_client(int socketfd) {
    int index; // Position of client in hashtable.
    // This is what is actually stored in the table. It contains the client too
    Node *node; 
    Client *client; // This is what we are trying to create

    // We use calloc because we want to zero out the memory on initialization.
    node = (Node *)calloc(1, sizeof(Node));

    // We need to allocate memory for the client.
    client = (Client *)calloc(1, sizeof(Client));

    // Initialize the client with some of its members default values.
    client->socketfd = socketfd;
    client->status = CONNECTED;
    client->buffer_max_size = 0;
    client->control_type = INVALID;
    client->data_type = INVALID;
    client->in_frame = false;
    
    // We are going to use socketfd as the hashtable key
    index = socketfd % HASHTABLE_SIZE;
    // Set up node members and add to table.
    node->client_socketfd = socketfd;
    node->client = client;
    node->next = clients_table[index];
    clients_table[index] = node;
    return client;
}

Client *get_client(int socketfd) {
    int index; // Index in the table
    Node *node;

    // We get the index of the client using the socket descriptor as 
    // key. We then search the linked list to get the node containing
    // the client.
    index = socketfd % HASHTABLE_SIZE;
    node = clients_table[index];
    while ( node != NULL ) {
        if ( node->client_socketfd == socketfd )
            break;
        node = node->next;
    }
    
    // If we can't find the client, then we return null.
    if ( node == NULL )
        return NULL;
    
    return node->client;
}

void delete_client(Client *client) {
    int index;
    // We need both of these variables to delete from the linked list.
    Node *prev, *current;

    index = client->socketfd % HASHTABLE_SIZE;

    prev = clients_table[index];
    // Nothing is found in table. That will be strange, but we want to be robust
    if ( prev == NULL ) {
        __free_client(client);
        return;
    }

    // If client node is at the beginning of the list. Set beginning of list to 
    // the next node. Free node and client.
    if ( prev->client == client ) {
        current = prev;
        clients_table[index] = current->next;
        __free_client(client);
        free(current);
        return;
    }

    // Search for client in list
    current = prev->next;
    while ( current != NULL ) {
        if ( current->client == client )
            break;
        prev = current;
        current = current->next;
    }

    // Free client, and if container node was found, free it too.
    __free_client(client);
    if ( current != NULL )
        free(current);
    return;
}

void __free_client(Client *client) {
    if ( client->control_data != NULL )
        free(client->control_data);

    if ( client->buffer != NULL )
        free(client->buffer);
    free(client);
}

void print_client(Client *client) {
    printf("Client info\n");
    printf("Socket: %d\n", client->socketfd);
    printf("Status: %d\n", client->status);
    printf("In final frame: %d\n", client->is_final_frame);
    printf("Is control frame: %d\n", client->is_control_frame);
    printf("Processing frame: %d\n", client->in_frame);
    printf("Header size: %d\n", client->header_size);
    printf("Payload size: %llu\n", client->payload_size);
    printf("Mask size: %d\n", client->mask_size);
    printf("Mask: %x%x%x%x\n", client->mask[0], client->mask[1], client->mask[2], client->mask[3]);
    printf("Control type: %d\n", client->control_type);
    printf("Control data: %s\n", client->control_data);
    printf("Control data size: %d\n", client->control_data_size);
    printf("Current data frame start: %llu\n", client->current_data_frame_start);
    printf("Buffer size: %llu\n", client->buffer_size);
    printf("Buffer max size: %llu\n", client->buffer_max_size);
}
