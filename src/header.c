#include "header.h"

#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"

void add_request(int socketfd, char buffer[], int buffer_len) {
  // We are going to use socketfd as the hashtable key
  int index = socketfd % INCOMPLETE_REQUEST_TABLE_SIZE;
  IncompleteRequest *incomplete_request = (IncompleteRequest *)malloc(sizeof(IncompleteRequest));
  incomplete_request->buffer_size = buffer_len;
  incomplete_request->socketfd = socketfd;
  memcpy(incomplete_request->buffer, buffer, buffer_len);

  incomplete_request->next = incomplete_request_table[index];
  incomplete_request_table[index] = incomplete_request;
}

IncompleteRequest *get_request(int socketfd) {
  // We get the index of the incomplete header struct using the socket descriptor as
  // key. We then search the linked list to get the incomplete_request containing
  // the client.
  int index = socketfd % INCOMPLETE_REQUEST_TABLE_SIZE;
  IncompleteRequest *incomplete_request = incomplete_request_table[index];
  while (incomplete_request != NULL) {
    if (incomplete_request->socketfd == socketfd) {
      break;
    }
    incomplete_request = incomplete_request->next;
  }
  return incomplete_request;
}

void delete_request(IncompleteRequest *request) {
  int index = request->socketfd % INCOMPLETE_REQUEST_TABLE_SIZE;

  IncompleteRequest *prev = incomplete_request_table[index];
  IncompleteRequest *current = NULL;
  // Nothing is found in table. That will be strange, but we want to be robust
  if (prev == NULL) {
    free(request);
    return;
  }

  // If incomplete request is at the beginning of the list. Set beginning of list to the next incomplete request. Free
  // incomplete request
  if (prev->socketfd == request->socketfd) {
    current = prev;
    incomplete_request_table[index] = current->next;
    free(current);
    return;
  }

  // Search for client in list
  current = prev->next;
  while (current != NULL) {
    if (current->socketfd == request->socketfd) {
      prev->next = current->next;
      break;
    }
    prev = current;
    current = current->next;
  }

  if (current != NULL) {
    free(current);
  }
}

int16_t move_to_next_line(char *start) {
  char *p = start;
  // Look for carriage return
  while (*p != '\r') {
    p++;
  }
  // Invalid header if it isn't followed by '\n'
  if (*p != '\r' && *(p + 1) != '\n') {
    return -1;
  }

  p += 2;  // Move to the next line
  return p - start;
}

int16_t is_upgrade_header_valid(int socketfd, char *start) {
  char *p = start;

  // Search for the presence of the first non-whitespace character
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p == '\0' || *p == '\r' || *p == '\n') {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Upgrade header");
    return -1;
  }
  if (strncasecmp(p, "websocket", 9) != 0) {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Upgrade header value");
    return -1;
  }
  p += 9;
  return p - start;
}

int16_t get_sec_websocket_key_value(int socketfd, char *start, uint8_t key[]) {
  char c = '\0';
  char *p = start;
  int key_length = 22;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p == '\0' || *p == '\r' || *p == '\n') {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Sec-Websocket-Key header");
    return -1;
  }
  int i = 0;
  while (i < key_length && *p != '\0') {
    c = *p;
    // Allow only valid base64 characters for the first 22 characters
    if (!isalnum(c) && c != '+' && c != '/') {
      break;
    }
    key[i] = c;
    i++;
    p++;
  }
  if (i < key_length) {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Sec-Websocket-Key header value");
    return -1;
  }
  // Valid Sec-Websocket-Key value must end with 2 '=' characters
  if (*p != '=' || *(p + 1) != '=') {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Sec-Websocket-Key header value");
    return -1;
  }
  key[key_length] = '=';
  key[key_length + 1] = '=';
  p += 2;
  return p - start;
}

int16_t is_version_header_valid(int socketfd, char *start) {
  char *p = start;
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p == '\0' || *p == '\r' || *p == '\n') {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Sec-Websocket-Version header");
    return -1;
  }
  if (strncasecmp(p, "13", 2) != 0) {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Sec-Websocket-Version header value");
    return -1;
  }
  p += 2;
  return p - start;
}

int16_t get_subprotocols(int socketfd, char *start, char subprotocol[], int *subprotocol_len) {
  char *p = start;
  while (*p == ' ' || *p == '\t') {
    p++;
  }

  // Check that the value doesn't start with a comma or is empty.
  if (*p == '\0' || *p == '\r' || *p == '\n' || *p == ',') {
    send_error_response(socketfd, CLIENT_ERROR, "Invalid Sec-Websocket-Protocol header");
    return -1;
  }
  int i = 0;
  while (i < 99 && *p != '\0' && *p != ',' && *p != '\r' && *p != '\n') {
    subprotocol[i] = *p;
    i++;
    p++;
  }
  *subprotocol_len = i;
  subprotocol[i] = '\0';
  return p - start;
}

bool __add_extension_param_key(char c, ExtensionParam **current_params_addr) {
  ExtensionParam *prev_params;
  ExtensionParam *current_params = *current_params_addr;
  if (current_params == NULL) {
    return false;
  }
  while (current_params != NULL && current_params->value_type != EMPTY) {
    prev_params = current_params;
    current_params = current_params->next;
  }
  if (current_params == NULL) {
    prev_params->next = calloc(1, sizeof(ExtensionParam));
    current_params = prev_params->next;
  }
  if (c == ',') {
    current_params->value_type = BOOL;
    current_params->bool_type = true;
    current_params->is_last = true;
  }
  *current_params_addr = current_params;
  return true;
}

void __add_extension_param_value(char c, char *start, int8_t length, ExtensionParam **current_params_addr) {
  ExtensionParam *current_params = *current_params_addr;
  if (current_params->value_type != EMPTY) {
    current_params->next = calloc(1, sizeof(ExtensionParam));
    current_params = current_params->next;
  }
  bool is_digit = true;
  int i;
  for (i = 0; i < length; i++) {
    is_digit &= isdigit(*(start + i));
    if (!is_digit) {
      break;
    }
  }
  if (current_params->key.length == 0 && !is_digit) {
    current_params->key.length = length;
    current_params->key.start = start;
    if (c == ',' || c == ';') {
      current_params->value_type = BOOL;
      current_params->bool_type = true;
    }
  } else if (is_digit) {
    if (c == '=') {
      current_params->string_type.start = start;
      current_params->string_type.length = length;
      current_params->value_type = STRING;
    } else {
      for (i = 0; i < length; i++) {
        current_params->int_type = current_params->int_type * 10 + (*(start + i) - '0');
      }
      current_params->value_type = INT;
    }
  } else if (current_params->key.length > 0) {
    current_params->string_type.length = length;
    current_params->string_type.start = start;
    current_params->value_type = STRING;
  }
  if (c == ',') {
    current_params->is_last = true;
  }
  *current_params_addr = current_params;
}

int16_t parse_extensions(int socketfd, char *line, ExtensionList *extension_list) {
  bool IN_QUOTE = false;
  char *p = line;
  char error[] = "Invalid Sec-Websocket-Extensions header";
  while (*p == ' ' || *p == '\t') {
    p++;
  }
  if (*p == '\0' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';') {
    send_error_response(socketfd, CLIENT_ERROR, error);
    return -1;
  }
  char *start = NULL;
  int8_t length = 0;
  bool has_extension = false;
  bool success = false;
  char c = '\0';
  char key[EXTENSION_TOKEN_LENGTH + 1];
  ExtensionParam *current_params = NULL;
  while (*p != '\0' && *p != '\r' && *p != '\n') {
    c = *p;
    if (c == '"') {
      IN_QUOTE = !IN_QUOTE;
    } else if ((c == ' ' || c == '\t') && start == NULL && !IN_QUOTE) {
      p++;
      continue;
    } else if (IN_QUOTE || !(c == ',' || c == ';' || c == '=')) {
      if (start == NULL) {
        start = p;
      }
      length++;
    } else if (!has_extension) {
      if (length > EXTENSION_TOKEN_LENGTH || length == 0 || c == '=') {
        send_error_response(socketfd, CLIENT_ERROR, error);
        return -1;
      }
      strncpy(key, start, length);
      key[length] = '\0';
      start = NULL;
      length = 0;
      current_params = get_extension_params(extension_list, key, true);
      success = __add_extension_param_key(c, &current_params);
      if (!success) {
        send_error_response(socketfd, CLIENT_ERROR, error);
        return -1;
      }
      if (c != ',') {
        has_extension = true;
      }
    } else {
      if (length > EXTENSION_TOKEN_LENGTH) {
        send_error_response(socketfd, CLIENT_ERROR, error);
        return -1;
      }
      __add_extension_param_value(c, start, length, &current_params);
      if (c == ',') {
        has_extension = false;
      }
      start = NULL;
      length = 0;
    }
    p++;
  }
  if (start == NULL || IN_QUOTE) {
    return p - line;
  }
  if (length > EXTENSION_TOKEN_LENGTH) {
    send_error_response(socketfd, CLIENT_ERROR, error);
    return -1;
  }
  if (!has_extension) {
    strncpy(key, start, length);
    key[length] = '\0';
    current_params = get_extension_params(extension_list, key, true);
    success = __add_extension_param_key(',', &current_params);
    if (!success) {
      send_error_response(socketfd, CLIENT_ERROR, error);
      return -1;
    }
  } else {
    __add_extension_param_value(',', start, length, &current_params);
  }
  return p - line;
}

bool validate_headers(char buf[], uint16_t request_length, int socketfd, uint8_t key[], char subprotocol[],
                      int subprotocol_len, uint8_t **extension_indices, uint8_t *indices_count) {
  char *p = NULL;
  int8_t index = 0;
  int16_t progress = 0;
  char *headers[] = {"Host:",
                     "Upgrade:",
                     "Sec-Websocket-Key:",
                     "Sec-Websocket-Version:",
                     "Sec-Websocket-Protocol:",
                     "Sec-Websocket-Extensions:"};
  int8_t header_count = 6;
  bool required_headers_present[] = {false, false, false, false};  // Store required headers check status here.
  bool is_valid = true;
  p = buf;
  ExtensionList *list = get_extension_list(socketfd);
  while (*p != '\0' && (p - buf) < request_length && *p != '\r' && *p != '\n') {
    for (index = 0; index < header_count && (p - buf) < request_length; index++) {
      progress = 0;
      if (index < 4 && required_headers_present[index]) {
        continue;
      }
      if (strncasecmp(p, headers[index], strlen(headers[index])) != 0) {
        continue;
      }
      p += strlen(headers[index]);  // Bypass header
      if (index == 0) {
        // Host is available. That's all we want to know
        required_headers_present[index] = true;
      } else if (index == 1) {
        progress = is_upgrade_header_valid(socketfd, p);
        if (progress == -1) {
          is_valid = false;
          break;
        }
        required_headers_present[index] = true;
      } else if (index == 2) {
        progress = get_sec_websocket_key_value(socketfd, p, key);
        if (progress == -1) {
          is_valid = false;
          break;
        }
        required_headers_present[index] = true;
      } else if (index == 3) {
        progress = is_version_header_valid(socketfd, p);
        if (progress == -1) {
          is_valid = false;
          break;
        }
        required_headers_present[index] = true;
      } else if (index == 4) {
        progress = get_subprotocols(socketfd, p, subprotocol, &subprotocol_len);
        if (progress == -1) {
          is_valid = false;
          break;
        }
      } else {
        progress = parse_extensions(socketfd, p, list);
        if (progress == -1) {
          is_valid = false;
          break;
        }
        break;
      }
      p += progress;
    }
    if (!is_valid) {
      delete_extension_list(socketfd);
      return is_valid;
    }
    progress = move_to_next_line(p);
    if (progress == -1) {
      return false;
    }
    p += progress;
  }

  // If none of the required headers isn't there, return false
  index = 0;
  while (index < 4) {
    if (!required_headers_present[index]) {
      return false;
    }
    index++;
  }

  is_valid = validate_extension_list(socketfd, list, extension_indices, indices_count);
  delete_extension_list(socketfd);
  return is_valid;
}
