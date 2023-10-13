#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extension.h"

void register_extension(char *key, bool (*parse_offer)(int,char*,uint16_t),
                           uint16_t (*respond_to_offer)(int,char*),
                           bool (*validate_rsv)(int,bool,bool,bool),
                           bool (*process_data)(int,char*,int,char**,int*),
                           void (*close)(int)
                        ) {
    if ( extension_table == NULL ) {
        extension_table = (Extension *)malloc(sizeof(Extension));
        extension_table[0].key = strdup(key);
        extension_table[0].parse_offer = parse_offer;
        extension_table[0].respond_to_offer = respond_to_offer;
        extension_table[0].validate_rsv = validate_rsv;
        extension_table[0].process_data = process_data;
        extension_table[0].close = close;
    }
    else {
        extension_table = (Extension *)realloc(extension_table, sizeof(Extension)*(extension_count+1));
        extension_table[extension_count].key = strdup(key);
        extension_table[extension_count].parse_offer = parse_offer;
        extension_table[extension_count].respond_to_offer = respond_to_offer;
        extension_table[extension_count].validate_rsv = validate_rsv;
        extension_table[extension_count].process_data = process_data;
        extension_table[extension_count].close = close;
        extension_count++;
    }
}


ExtensionList *get_extension_list(int socketfd) {
    int index; // Index in the table

    // We get the index of the client using the socket descriptor as 
    // key. We then search the linked list to get the node containing
    // the client.
    index = socketfd % WAITING_CLIENT_TABLE_SIZE;
    WaitingClient* client = waiting_clients_table[index];
    while ( client != NULL ) {
        if ( client->socketfd == socketfd )
            break;
        client = client->next;
    }
    
    // If we can't find the client, then create new waiting client.
    if ( client == NULL ) {
        client = (WaitingClient *) malloc(sizeof(WaitingClient));
        client->next = waiting_clients_table[index];
        client->extensions = (ExtensionList *) calloc(1, sizeof(ExtensionList));
        client->socketfd = socketfd;
    }
    return client->extensions;
}


void delete_extensions(int socketfd) {
    int index;
    // We need both of these variables to delete from the linked list.
    WaitingClient *prev, *current;

    index = socketfd % WAITING_CLIENT_TABLE_SIZE;

    prev = waiting_clients_table[index];

    // Nothing is found in table. That will be strange.
    if ( prev == NULL ) {
        return;
    }

    // If node is at the beginning of the list. Set beginning of list to 
    // the next node. Free node.
    if ( prev->socketfd == socketfd ) {
        current = prev;
        waiting_clients_table[index] = current->next;
        free(prev->extensions);
        free(prev);
        return;
    }

    // Search for client in list
    current = prev->next;
    while ( current != NULL ) {
        if ( current->socketfd == socketfd ) {
            prev->next = current->next;
            break;
        }
        prev = current;
        current = current->next;
    }
    if ( current != NULL ) {
        free(current->extensions);
        free(current);
    }
    return;
}

char* get_extension_params(ExtensionList *list, char *key, bool create) {
    if ( strlen(list->key) == 0 ) {
        strcpy(list->key, key);
        return list->value;
    }

    ExtensionList *prev = list;
    while ( list != NULL ) {
        if ( strcmp(list->key, key) == 0 ) {
            return list->value;
        }
        prev = list;
        list = list->next;
    }

    if ( create ) {
        if ( list == NULL ) {
            prev->next = (ExtensionList *)calloc(1, sizeof(ExtensionList));
            list = prev->next;
        }
        strcpy(list->key, key);
        return list->value;
    }
    return NULL;
}

void print_list(ExtensionList *list) {
    while ( list != NULL ) {
        printf("Key: %s\n", list->key);
        printf("Value: %s\n", list->value);
        list = list->next;
    }
}