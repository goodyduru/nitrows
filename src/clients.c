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
        free_frames(client);
        free(client);
        return;
    }

    // If client node is at the beginning of the list. Set beginning of list to 
    // the next node. Free node and client.
    if ( prev->client == client ) {
        current = prev;
        clients_table[index] = current->next;
        free_frames(client);
        free(client);
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

    // Free client and its frames
    free_frames(client);
    free(client);
    if ( current != NULL )
        free(current);
    return;
}

void free_frames(Client *client) {
    // Free current frame if it's a control frame.
    if ( client->current_frame != NULL && client->current_frame->is_control ) {
        free(client->current_frame);
    }
    Frame *current = client->data_frames;
    Frame *next;
    while ( current != NULL ) {
        if ( current->buffer != NULL )
            free(current->buffer);
        next = current->next;
        free(current);
        current = next;
    }
    
    client->data_frames = NULL;
    client->current_frame = NULL;
}

void print_client(Client *client) {
    printf("Client info\n");
    printf("Socket: %d\n", client->socketfd);
    printf("Status: %d\n", client->status);
    printf("Processing frame: %d\n", client->in_frame);
    printf("Header size: %d\n", client->header_size);
    printf("Mask size: %d\n", client->mask_size);
    printf("Mask: %x%x%x%x\n", client->mask[0], client->mask[1], client->mask[2], client->mask[3]);
    if ( client->current_frame != NULL ) {
        printf("Type: %d\n", client->current_frame->type);
        printf("Data size: %llu\n", client->current_frame->payload_size);
        printf("Buffer size: %llu\n", client->current_frame->buffer_size);
        printf("Is final %d\n", client->current_frame->is_final);
    }
    if ( client->data_frames != NULL ) {
        printf("Type: %d\n", client->data_frames->type);
        printf("Data size: %llu\n", client->data_frames->payload_size);
        printf("Buffer size: %llu\n", client->data_frames->buffer_size);
        printf("Is final %d\n", client->data_frames->is_final);
    }
}
