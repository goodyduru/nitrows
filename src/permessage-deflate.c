#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "permessage-deflate.h"

void pmd_add_to_table(PMDClientConfig *config) {
    // We are going to use socketfd as the hashtable key
    int index = config->socketfd % HASHTABLE_SIZE;
    config->next = pmd_config_table[index];
    pmd_config_table[index] = config;
}

void pmd_delete_from_table(int socketfd) {
    int index = socketfd % HASHTABLE_SIZE;
    PMDClientConfig *prev, *config = pmd_config_table[index];
    if ( config == NULL ) {
        return;
    }
    if ( config->socketfd == socketfd ) {
        pmd_config_table[index] = config->next;
        return;
    }
    prev = config;
    while ( config != NULL ) {
        if ( config->socketfd == socketfd ) {
            prev->next = config->next;
            free(config);
            break;
        }
        prev = config;
        config = config->next;
    }
}

PMDClientConfig* pmd_get_from_table(int socketfd) {
    int index = socketfd % HASHTABLE_SIZE;
    PMDClientConfig *config = pmd_config_table[index];
    while ( config != NULL ) {
        if ( config->socketfd == socketfd ) {
            break;
        }
        config = config->next;
    }
    return config;
}

bool pmd_validate_offer(int socketfd, ExtensionParam* param) {
    bool has_seen_server_context_takeover = false;
    bool has_seen_client_context_takeover = false;
    bool has_seen_client_max_window_bits = false;
    bool has_seen_server_max_window_bits = false;
    bool acceptable = true;
    bool client_no_context_takeover = false;
    bool server_no_context_takeover = false;
    uint8_t client_max_window_bits = MAX_WINDOW_BITS;
    uint8_t server_max_window_bits = MAX_WINDOW_BITS;
    PMDClientConfig *config = calloc(1, sizeof(PMDClientConfig));

    while ( param != NULL ) {
        if ( strlen(param->key) == 0 ) {
            config->socketfd = socketfd;
            config->client_max_window_bits = MAX_WINDOW_BITS;
            config->server_max_window_bits = MAX_WINDOW_BITS;
            config->client_no_context_takeover = DEFAULT_NO_CONTEXT_TAKEOVER;
            config->server_no_context_takeover = DEFAULT_NO_CONTEXT_TAKEOVER;
            acceptable = true;
            pmd_add_to_table(config);
            break;
        }
        if ( strcasecmp("client_max_window_bits", param->key) == 0 ) {
            if ( has_seen_client_max_window_bits || 
                param->value_type == STRING ||
                (param->value_type == INT && (param->int_type < 8 || param->int_type > 15)) ) {
                acceptable &= false;
                break;
            }
            if ( param->value_type == BOOL ) {
                client_max_window_bits = 15;
            } else {
                client_max_window_bits = param->int_type;
            }
            has_seen_client_max_window_bits = true;
        } else if ( strcasecmp("server_max_window_bits", param->key) == 0 ) {
            if ( has_seen_server_max_window_bits ||
                 param->value_type == STRING ||
                (param->value_type == INT && (param->int_type < 8 || param->int_type > 15))  ) {
                acceptable &= false;
                break;
            }
            if ( param->value_type == BOOL ) {
                server_max_window_bits = 15;
            } else {
                server_max_window_bits = param->int_type;
            }
            has_seen_server_max_window_bits = true;
        } else if ( strcasecmp("client_no_context_takeover", param->key) == 0) {
            if (has_seen_client_context_takeover || param->value_type != BOOL) {
                acceptable &= false;
                break;
            }
            client_no_context_takeover = true;
            has_seen_client_context_takeover = true;
        } else if ( strcasecmp("server_no_context_takeover", param->key) == 0) {
            if (has_seen_server_context_takeover || param->value_type != BOOL) {
                acceptable &= false;
                break;
            }
            server_no_context_takeover = true;
            has_seen_server_context_takeover = true;
        }
        else {
            acceptable &= false;
            break;
        }
        if ( acceptable && param->is_last ) {
            config->socketfd = socketfd;
            config->client_max_window_bits = client_max_window_bits;
            config->server_max_window_bits = server_max_window_bits;
            config->client_no_context_takeover = client_no_context_takeover;
            config->server_no_context_takeover = server_no_context_takeover;
            pmd_add_to_table(config);
            break;
        } else if ( !acceptable && param->is_last && param->next != NULL ) {
            has_seen_server_context_takeover = false;
            has_seen_client_context_takeover = false;
            has_seen_client_max_window_bits = false;
            has_seen_server_max_window_bits = false;
            acceptable = true;
            client_no_context_takeover = false;
            server_no_context_takeover = false;
            client_max_window_bits = 0;
            server_max_window_bits = 0;
        }
        param = param->next;
    } 
    if ( !acceptable ) {
        free(config);
    }
    return acceptable;
}

uint16_t pmd_respond(int socketfd, char *response) {
    PMDClientConfig *config = pmd_get_from_table(socketfd);
    if ( config == NULL ) {
        return 0;
    }
    strcpy(response, "permessage-deflate");
    uint16_t length = 18;
    if ( config->client_max_window_bits != MAX_WINDOW_BITS ) {
        length += sprintf(response+length, "; client_max_window_bits=%d",
                            config->client_max_window_bits);
    }
    if ( config->server_max_window_bits != MAX_WINDOW_BITS ) {
        length += sprintf(response+length, "; server_max_window_bits=%d",
                            config->server_max_window_bits);
    }
    if ( config->client_no_context_takeover != DEFAULT_NO_CONTEXT_TAKEOVER ) {
        length += sprintf(response+length, "; client_no_context_takeover");
    }
    if ( config->server_no_context_takeover != DEFAULT_NO_CONTEXT_TAKEOVER ) {
        length += sprintf(response+length, "; server_no_context_takeover");
    }
    response[length] = '\0';
    return length;
}

void pmd_close(int socketfd) {
    pmd_delete_from_table(socketfd);
}