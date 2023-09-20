#include <stdlib.h>

#include "./clients.h"

wsclient *init_wsclient(int socketfd) {
    int index; // Position of client in hashtable.
    // This is what is actually stored in the table. It contains the client too
    wsclient_node *node; 
    wsclient *client; // This is what we are trying to create

    // We use calloc because we want to zero out the memory on initialization.
    node = (wsclient_node *)calloc(1, sizeof(wsclient_node));

    // We need to allocate memory for the client including its buffer. 
    // We can always increase the size of the buffer as data is received
    client = (wsclient *)calloc(1, sizeof(wsclient) + BUFFER_SIZE);

    // Initialize the client with some of its members default values.
    client->socketfd = socketfd;
    client->status = CONNECTED;
    client->buffer_limit = BUFFER_SIZE;
    
    // We are going to use socketfd as the hashtable key
    index = socketfd % HASHTABLE_SIZE;
    // Set up node members and add to table.
    node->client_socketfd = socketfd;
    node->client = client;
    node->next = clients_table[index];
    clients_table[index] = node;
    return client;
}

wsclient *get_wsclient(int socketfd) {
    int index; // Index in the table
    wsclient_node *node;

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

void delete_wsclient(wsclient *client) {
    int socketfd, index;
    // We need both of these variables to delete from the linked list.
    wsclient_node *prev, *current;

    index = client->socketfd % HASHTABLE_SIZE;

    prev = clients_table[index];
    // Nothing is found in table. That will be strange, but we want to be robust
    if ( prev == NULL ) {
        __free_wsclient(client);
        return;
    }

    // If client node is at the beginning of the list. Set beginning of list to 
    // the next node. Free node and client.
    if ( prev->client == client ) {
        current = prev;
        clients_table[index] = current->next;
        __free_wsclient(client);
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
    __free_wsclient(client);
    if ( current != NULL )
        free(current);
    return;
}

void __free_wsclient(wsclient *client) {
    if ( client->control_extra_data != NULL )
        free(client->control_extra_data);
    free(client->buffer);
    free(client);
}
