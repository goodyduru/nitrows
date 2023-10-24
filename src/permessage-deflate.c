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
            if ( config->inflater != NULL ) {
                (void)inflateEnd(config->inflater);
                free(config->inflater);
            }
            if ( config->deflater != NULL ) {
                (void)deflateEnd(config->deflater);
                free(config->deflater);
            }
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

bool pmd_process_data(int socketfd, Frame* frame, uint8_t **output,
                        uint64_t *output_length) {
    if ( frame->rsv1 == 0 ) {
        return true;
    }
    PMDClientConfig *config = pmd_get_from_table(socketfd);
    if ( config == NULL ) {
        return false;
    }
    int ret;
    if ( config->inflater == NULL ) {
        config->inflater = (z_stream *) malloc(sizeof(z_stream));
        config->inflater->zalloc = Z_NULL;
        config->inflater->zfree = Z_NULL;
        config->inflater->opaque = Z_NULL;
        config->inflater->avail_in = 0;
        config->inflater->next_in = Z_NULL;
        ret = inflateInit2(config->inflater, -config->client_max_window_bits);
        if ( ret != Z_OK )
            return false;
    }
    z_stream *inflater = config->inflater;
    uint8_t *out = *output;
    uint64_t length = *output_length;
    uint64_t input_size = 0;
    uint64_t written = 0;
    input_size = frame->buffer_size;

    if ( input_size == 0 ) {
        return true;
    }
    printf("Payload size: %llu\n", input_size);
    inflater->avail_in = input_size;
    inflater->next_in = frame->buffer;
    do {
        if ( out == NULL ) {
            // typically output is at least twice the size of input.
            length = 2*input_size; 
            out = malloc(length); 
        }
        else {
            length *= 2;
            out = realloc(out, length);
        }
        inflater->avail_out = length - written;
        inflater->next_out = out+written;
        ret = inflate(inflater, Z_NO_FLUSH);
        printf("%llu, %llu, %u\n", length, written, inflater->avail_out);
        written = length - inflater->avail_out;
        printf("Inflate ret: %d\n", ret);
        if ( ret == Z_STREAM_END || (ret == Z_OK && inflater->avail_out > 0) ) {
            if ( inflater->avail_out < 8 ) {
                length += 8; // This should be more than enough
                out = realloc(out, length);
            }
            inflater->avail_out = length - written;
            inflater->next_out = out+written;
            inflater->avail_in = 4; // trailer bytes
            inflater->next_in = (uint8_t *) TRAILER;
            ret = inflate(inflater, Z_NO_FLUSH);
            if ( ret != Z_STREAM_END && ret != Z_OK ) {
                return false;
            }
            written = length - inflater->avail_out;
        } else if ( ret != Z_OK && ret != Z_BUF_ERROR ) {
            return false;
        }
    } while ( inflater->avail_out == 0 );

    if ( config->client_no_context_takeover ) {
        inflateReset(inflater);
    }
    *output = out;
    *output_length = written;
    return true;
}

uint64_t pmd_generate_response(int socketfd, uint8_t* input,
                                uint64_t input_length, Frame* output_frame) {
    PMDClientConfig *config = pmd_get_from_table(socketfd);
    if ( config == NULL ) {
        return 0;
    }
    int ret;
    if ( config->deflater == NULL ) {
        config->deflater = (z_stream *) malloc(sizeof(z_stream));
        config->deflater->zalloc = Z_NULL;
        config->deflater->zfree = Z_NULL;
        config->deflater->opaque = Z_NULL;
        ret = deflateInit2(config->deflater, Z_DEFAULT_COMPRESSION, 
        Z_DEFLATED, -config->server_max_window_bits, 8, Z_DEFAULT_STRATEGY);
        if ( ret != Z_OK )
            return 0;
    }
    z_stream *deflater = config->deflater;
    uint64_t written = 0;
    uint8_t *out = NULL;
    uint64_t length;
    deflater->avail_in = input_length;
    deflater->next_in = input;
    do {
        if ( out == NULL ) {
            // typically output is at least twice the size of input.
            length = input_length; 
            out = malloc(length); 
        }
        else {
            length *= 2;
            out = realloc(out, length);
        }
        deflater->avail_out = length - written;
        deflater->next_out = out+written;
        ret = deflate(deflater, Z_SYNC_FLUSH);
        printf("Deflate ret: %d\n", ret);
        written = length - deflater->avail_out;
        printf("Deflate numbers: %llu, %llu, %u\n", length, written, deflater->avail_out);
        if ( ret != Z_OK && ret != Z_STREAM_END ) {
            printf("Error %d\n", ret);
            return 0;
        }
    } while ( deflater->avail_out == 0 );
    if ( config->server_no_context_takeover ) {
        deflateReset(deflater);
    }
    output_frame->buffer = out;
    output_frame->buffer_size = written - 4; // Remove trailing bits
    output_frame->payload_size = written - 4; // Remove trailing bits
    output_frame->rsv1 = true;
    return written - 4;
}

void pmd_close(int socketfd) {
    pmd_delete_from_table(socketfd);
}