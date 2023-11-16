#include <stdio.h>

#include "nitrows.h"

void echo_message(int key, uint8_t *message, uint64_t length) {
  bool is_sent = nitrows_send_message(key, message, length);
  if (!is_sent) {
    nitrows_close(key);
  }
}

int main() {
  nitrows_set_message_handler(echo_message);
  nitrows_run();
}
