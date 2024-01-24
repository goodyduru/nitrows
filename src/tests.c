#include <stdio.h>

#include "header.h"
#include "nitrows.h"

void echo_message(int client_id, uint8_t *message, uint64_t length) {
  bool is_sent = nitrows_send_message(client_id, message, length);
  if (!is_sent) {
    nitrows_close(client_id);
  }
}

int test_extension_parser() {
  char tokens[] = "foo,bar;baz,foo;baz";
  char tokens1[] = "foo;bar;baz=1;bar=2";
  char tokens2[] = "bar;quote=\"10\",gaga";
  ExtensionList *list = get_extension_list(1);
  parse_extensions(1, tokens, list);
  parse_extensions(1, tokens1, list);
  parse_extensions(1, tokens2, list);
  printf("Testing tokens: %s\n", tokens);
  print_list(list);
  return 0;
}

int main() {
  nitrows_set_message_handler(echo_message);
  nitrows_run();
}
