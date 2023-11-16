#include "nitrows.h"

#include <stdlib.h>
#include <string.h>

#include "events.h"
#include "extension.h"
#include "frame.h"
#include "handlers.h"
#include "net.h"
#include "permessage-deflate.h"
#include "server.h"

void nitrows_register_extension(char *key, bool (*validate_offer)(int, ExtensionParam *),
                                uint16_t (*respond_to_offer)(int, char *),
                                bool (*process_data)(int, Frame *, uint8_t **, uint64_t *),
                                uint64_t (*generate_data)(int, uint8_t *, uint64_t, Frame *), void (*close)(int)) {
  register_extension(key, validate_offer, respond_to_offer, process_data, generate_data, close);
}

void nitrows_set_message_handler(void (*handle_message)(int, uint8_t *, uint64_t)) {
  set_message_handler(handle_message);
}

bool nitrows_send_message(int key, uint8_t *message, uint64_t length) { return send_data_frame(key, message, length); }

void nitrows_close(int key) { start_closing(key); }

void nitrows_run() {
  nitrows_register_extension("permessage-deflate", pmd_validate_offer, pmd_respond, pmd_process_data,
                             pmd_generate_response, pmd_close);
  int listener_socket = get_listener_socket();
  init_event_loop();
  add_to_event_loop(listener_socket);
  run_event_loop(listener_socket, accept_connection, handle_connection);
}