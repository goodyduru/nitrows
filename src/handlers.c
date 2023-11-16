#include "handlers.h"

void set_message_handler(void (*handle_message)(int, uint8_t *, uint64_t)) {
  nitrows_handler.handle_message = handle_message;
}

NitrowsHandler *get_handlers() { return &nitrows_handler; }