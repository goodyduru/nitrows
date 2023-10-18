#ifndef INCLUDED_PERMESSAGE_DEFLATE_DOT_H
#define INCLUDED_PERMESSAGE_DEFLATE_DOT_H

#include "defs.h"
#include "extension.h"

#define MAX_WINDOW_BITS 15
#define DEFAULT_NO_CONTEXT_TAKEOVER false

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
    PMDClientConfig *next;
};

// Table containing all the connected clients config.
PMDClientConfig *pmd_config_table[HASHTABLE_SIZE];

bool pmd_validate_offer(int socketfd, ExtensionParam* param);
uint16_t pmd_respond(int socketfd, char *response);
bool pmd_validate_rsv(int socketfd, bool rsv1, bool rsv2, bool rsv3);
bool pmd_process_data(int socketfd, Frame* frames, int frame_count,
                        char **response, int *response_length);
void pmd_close(int socketfd);

#endif