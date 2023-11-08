#include <stdio.h>

#include "nitrows.h"

bool echo_message(int key, uint8_t *message, uint64_t length) { return nitrows_send_message(key, message, length); }

int main() {
  nitrows_set_message_handler(echo_message);
  nitrows_run();
}
