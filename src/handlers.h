#ifndef NITROWS_SRC_HANDLERS_H
#define NITROWS_SRC_HANDLERS_H

#include <stdbool.h>
#include <stdint.h>

typedef struct NitrowsHandler NitrowsHandler;

struct NitrowsHandler {
    bool (*handle_message)(int, uint8_t *, uint64_t);
};

static NitrowsHandler nitrows_handler;

void set_message_handler(bool (*handle_message)(int, uint8_t *, uint64_t));

NitrowsHandler *get_handlers();
#endif