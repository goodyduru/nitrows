#include "extension.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void register_extension(char *key, bool (*validate_offer)(int, ExtensionParam *),
                        uint16_t (*respond_to_offer)(int, char *),
                        bool (*process_data)(int, Frame *, uint8_t **, uint64_t *),
                        uint64_t (*generate_data)(int, uint8_t *, uint64_t, Frame *), void (*close)(int)) {
  if (extension_count == 0) {
    extension_table = (Extension *)malloc(sizeof(Extension));
    extension_table[0].key = strdup(key);
    extension_table[0].validate_offer = validate_offer;
    extension_table[0].respond_to_offer = respond_to_offer;
    extension_table[0].process_data = process_data;
    extension_table[0].generate_data = generate_data;
    extension_table[0].close = close;
    extension_count = 1;
  } else {
    Extension *temp;
    temp = (Extension *)realloc(extension_table, sizeof(Extension) * (extension_count + 1));
    if (temp == NULL) {
      free(extension_table);
      exit(1);
    }
    extension_table = temp;
    extension_table[extension_count].key = strdup(key);
    extension_table[extension_count].validate_offer = validate_offer;
    extension_table[extension_count].respond_to_offer = respond_to_offer;
    extension_table[extension_count].process_data = process_data;
    extension_table[extension_count].generate_data = generate_data;
    extension_table[extension_count].close = close;
    extension_count++;
  }
}

Extension *get_extension(uint8_t index) {
  if (index >= extension_count) {
    return NULL;
  }
  return &extension_table[index];
}

int16_t find_extension_functions(char *key) {
  if (extension_count == 0) {
    return -1;
  }
  for (int8_t i = 0; i < extension_count; i++) {
    if (strcmp(key, extension_table[i].key) == 0) {
      return i;
    }
  }
  return -1;
}

ExtensionList *get_extension_list(int socketfd) {
  // We get the index of the client using the socket descriptor as
  // key. We then search the linked list to get the node containing
  // the client.
  int index = socketfd % WAITING_CLIENT_TABLE_SIZE;
  WaitingClient *client = waiting_clients_table[index];
  while (client != NULL) {
    if (client->socketfd == socketfd) {
      break;
    }
    client = client->next;
  }

  // If we can't find the client, then create new waiting client.
  if (client == NULL) {
    client = (WaitingClient *)malloc(sizeof(WaitingClient));
    client->next = waiting_clients_table[index];
    client->extensions = (ExtensionList *)calloc(1, sizeof(ExtensionList));
    client->socketfd = socketfd;
    waiting_clients_table[index] = client;
  }
  return client->extensions;
}

void delete_extension_list(int socketfd) {
  int index = socketfd % WAITING_CLIENT_TABLE_SIZE;

  WaitingClient *prev = waiting_clients_table[index];
  WaitingClient *current = NULL;

  // Nothing is found in table. That will be strange.
  if (prev == NULL) {
    return;
  }

  // If node is at the beginning of the list. Set beginning of list to
  // the next node. Free node.
  if (prev->socketfd == socketfd) {
    current = prev;
    waiting_clients_table[index] = current->next;
    free_extension_list(prev->extensions);
    free(prev);
    return;
  }

  // Search for client in list
  current = prev->next;
  while (current != NULL) {
    if (current->socketfd == socketfd) {
      prev->next = current->next;
      break;
    }
    prev = current;
    current = current->next;
  }
  if (current != NULL) {
    free_extension_list(current->extensions);
    free(current);
  }
}

void free_extension_list(ExtensionList *list) {
  ExtensionList *next = NULL;
  ExtensionParam *current = NULL;
  ExtensionParam *next_param = NULL;
  while (list != NULL) {
    next = list->next;
    current = list->params;
    while (current != NULL) {
      next_param = current->next;
      free(current);
      current = next_param;
    }
    free(list);
    list = next;
  }
}

bool validate_extension_list(int socketfd, ExtensionList *list, uint8_t **extension_indices, uint8_t *indices_count) {
  if (list == NULL || strlen(list->token) == 0 || extension_count == 0) {
    return true;
  }
  bool is_valid = true;
  int16_t found = 0;
  uint8_t count = *indices_count;
  uint8_t *indices = *extension_indices;
  uint8_t *temp;
  uint8_t size = 0;
  while (list != NULL) {
    found = find_extension_functions(list->token);
    if (found == -1) {
      list = list->next;
      continue;
    }
    is_valid = extension_table[found].validate_offer(socketfd, list->params);
    if (!is_valid) {
      if (indices != NULL) {
        free(indices);
      }
      return false;
    }
    if (indices == NULL || size == 0) {
      indices = (uint8_t *)malloc(sizeof(uint8_t));
      indices[count] = found;
      size = 1;
      count = 1;
    } else {
      if (count == size) {
        if (size == 255) {
          break;
        }
        size *= 2;
        temp = (uint8_t *)realloc(indices, size);
        if (temp == NULL) {
          free(indices);
          return false;
        }
        indices = temp;
      }
      indices[count] = found;
      count++;
    }
    list = list->next;
  }
  *extension_indices = indices;
  *indices_count = count;
  return true;
}

ExtensionParam *get_extension_params(ExtensionList *list, char *key, bool create) {
  if (strlen(list->token) == 0 && create) {
    strncpy(list->token, key, EXTENSION_TOKEN_LENGTH);
    list->token[EXTENSION_TOKEN_LENGTH] = '\0';
    list->params = (ExtensionParam *)calloc(1, sizeof(ExtensionParam));
    list->params->value_type = EMPTY;
    return list->params;
  }

  ExtensionList *prev = list;
  while (list != NULL) {
    if (strcmp(list->token, key) == 0) {
      return list->params;
    }
    prev = list;
    list = list->next;
  }

  if (create) {
    if (list == NULL) {
      prev->next = (ExtensionList *)calloc(1, sizeof(ExtensionList));
      list = prev->next;
    }
    strncpy(list->token, key, EXTENSION_TOKEN_LENGTH);
    list->token[EXTENSION_TOKEN_LENGTH] = '\0';
    list->params = (ExtensionParam *)calloc(1, sizeof(ExtensionParam));
    list->params->value_type = EMPTY;
    return list->params;
  }
  return NULL;
}

void print_list(ExtensionList *list) {
  ExtensionParam *param = NULL;
  char truthy[] = "true";
  char falsy[] = "false";
  char key[EXTENSION_TOKEN_LENGTH + 1];
  char value[EXTENSION_TOKEN_LENGTH + 1];
  while (list != NULL) {
    printf("Key: %s\n", list->token);
    param = list->params;
    while (param != NULL) {
      strncpy(key, param->key.start, param->key.length);
      key[param->key.length] = '\0';
      switch (param->value_type) {
        case BOOL:
          printf("\t%s=%s\n", key, (param->bool_type ? truthy : falsy));
          break;
        case INT:
          printf("\t%s=%lld\n", key, param->int_type);
          break;
        case STRING:
          strncpy(value, param->string_type.start, param->string_type.length);
          value[param->string_type.length] = '\0';
          printf("\t%s=%s\n", key, value);
        default:
          printf("Empty\n");
      }
      if (param->is_last && param->next != NULL) {
        printf("Another set\n");
      }
      param = param->next;
    }
    list = list->next;
  }
}