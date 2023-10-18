#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "extension.h"

void register_extension(char *key, bool (*validate_offer)(int,ExtensionParam*),
                           uint16_t (*respond_to_offer)(int,char*),
                           bool (*process_data)(int,Frame*,int,char**,int*),
                           void (*close)(int)
                        ) {
    if ( extension_table == NULL ) {
        extension_table = (Extension *)malloc(sizeof(Extension));
        extension_table[0].key = strdup(key);
        extension_table[0].validate_offer = validate_offer;
        extension_table[0].respond_to_offer = respond_to_offer;
        extension_table[0].process_data = process_data;
        extension_table[0].close = close;
        extension_count = 1;
    }
    else {
        extension_table = (Extension *)realloc(extension_table, sizeof(Extension)*(extension_count+1));
        extension_table[extension_count].key = strdup(key);
        extension_table[extension_count].validate_offer = validate_offer;
        extension_table[extension_count].respond_to_offer = respond_to_offer;
        extension_table[extension_count].process_data = process_data;
        extension_table[extension_count].close = close;
        extension_count++;
    }
}

int16_t find_extension_functions(char *key) {
    if ( extension_count == 0 ) {
        return -1;
    }
    for ( int8_t i = 0; i < extension_count; i++ ) {
        if ( strcmp(key, extension_table[i].key) == 0 ) {
            return i;
        }
    }
    return -1;
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


void delete_extension_list(int socketfd) {
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
        free_extension_list(prev->extensions);
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
        free_extension_list(current->extensions);
        free(current);
    }
    return;
}

void free_extension_list(ExtensionList *list) {
    ExtensionList *next;
    ExtensionParam *current, *next_param;
    while ( list != NULL ) {
        next = list->next;
        current = list->params;
        while ( current != NULL ) {
            next_param = current->next;
            free(current);
            current = next_param;
        }
        free(list);
        list = next;
    }
}

bool validate_extension_list(int socketfd, ExtensionList *list,
                             uint8_t **extension_indices,
                             uint8_t *indices_count) {
    if ( list == NULL || strlen(list->token) == 0 || extension_count == 0 ) {
        return true;
    }
    bool is_valid = true;
    int16_t found;
    uint8_t count = *indices_count;
    uint8_t *indices = *extension_indices;
    uint8_t size;
    while ( list != NULL ) {
        found = find_extension_functions(list->token);
        if ( found == -1 ) {
            list = list->next;
            continue;
        }
        is_valid = extension_table[found].validate_offer(socketfd, list->params);
        if ( !is_valid ) {
            if ( indices != NULL ) {
                free(indices);
            }
            return false;
        }
        if ( indices == NULL ) {
            indices = (uint8_t *) malloc(sizeof(uint8_t));
            indices[count] = found;
            size = 1;
            count = 1;
        }
        else {
            if ( count == size ) {
                if ( size == 255 ) {
                    break;
                }
                size *= 2;
                indices = (uint8_t *) malloc(sizeof(uint8_t)*size);
            }
            indices[count] = found;
            count++;
        }
        list = list->next;
    }
    *extension_indices = indices;
    *indices_count = count;
    return true;
}

ExtensionParam* get_extension_params(ExtensionList *list, char *key, bool create) {
    if ( strlen(list->token) == 0 && create ) {
        strncpy(list->token, key, EXTENSION_TOKEN_LENGTH);
        list->token[EXTENSION_TOKEN_LENGTH]  = '\0';
        list->params = (ExtensionParam *)calloc(1, sizeof(ExtensionParam));
        list->params->value_type = EMPTY;
        return list->params;
    }

    ExtensionList *prev = list;
    while ( list != NULL ) {
        if ( strcmp(list->token, key) == 0 ) {
            return list->params;
        }
        prev = list;
        list = list->next;
    }

    if ( create ) {
        if ( list == NULL ) {
            prev->next = (ExtensionList *)calloc(1, sizeof(ExtensionList));
            list = prev->next;
        }
        strncpy(list->token, key, EXTENSION_TOKEN_LENGTH);
        list->token[EXTENSION_TOKEN_LENGTH] = '\0';
        list->params = (ExtensionParam *)calloc(1, sizeof(ExtensionParam));
        list->params->value_type = EMPTY;
        return list->params;
    }
    return NULL;
}

void print_list(ExtensionList *list) {
    ExtensionParam *param;
    char truthy[] = "true";
    char falsy[] = "false";
    while ( list != NULL ) {
        printf("Key: %s\n", list->token);
        param = list->params;
        while ( param != NULL ) {
            switch ( param->value_type ) {
                case BOOL:
                    printf("\t%s=%s\n", param->key, (param->bool_type ? truthy : falsy));
                    break;
                case INT:
                    printf("\t%s=%lld\n", param->key, param->int_type);
                    break;
                case STRING:
                    printf("\t%s=%s\n", param->key, param->string_type);
                default:
                    printf("Empty\n");
            }
            if ( param->is_last && param->next != NULL ) {
                printf("Another set\n");
            }
            param = param->next;
        }
        list = list->next;
    }
}