#ifndef NITROWS_SRC_PERMESSAGE_DEFLATE_H
#define NITROWS_SRC_PERMESSAGE_DEFLATE_H

#include <zlib.h>

#include "defs.h"
#include "extension.h"

#define MAX_WINDOW_BITS 15
#define DEFAULT_NO_CONTEXT_TAKEOVER false
#define CHUNK 16384
#define TRAILER "\x00\x00\xff\xff"

/**
 * Config for a single client
*/
typedef struct pmd_client_config PMDClientConfig;

struct pmd_client_config {
    int socketfd;
    uint8_t server_max_window_bits;
    uint8_t client_max_window_bits;
    bool server_no_context_takeover;
    bool client_no_context_takeover;
    z_stream *inflater;
    z_stream *deflater;
    PMDClientConfig *next;
};

// Table containing all the connected clients config.
static PMDClientConfig *pmd_config_table[HASHTABLE_SIZE];

bool pmd_validate_offer(int socketfd, ExtensionParam* param);
uint16_t pmd_respond(int socketfd, char *response);
bool pmd_validate_rsv(int socketfd, bool rsv1, bool rsv2, bool rsv3);
bool pmd_process_data(int socketfd, Frame* frame, uint8_t **output,
                        uint64_t *output_length);
uint64_t pmd_generate_response(int socketfd, uint8_t* input, uint64_t input_length,
                                Frame* output_frame);
void pmd_close(int socketfd);

#endif