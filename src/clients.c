#include "./clients.h"

#include <stdio.h>
#include <stdlib.h>

#include "extension.h"

Client *init_client(int socketfd, uint8_t *extension_indices, uint8_t indices_count) {
  // We are going to use socketfd as the hashtable key
  int index = socketfd % HASHTABLE_SIZE;
  // This is what is actually stored in the table. It contains the client too
  // We use calloc because we want to zero out the memory on initialization.
  Node *node = (Node *)calloc(1, sizeof(Node));

  // We need to allocate memory for the client.
  Client *client = (Client *)calloc(1, sizeof(Client));

  // Initialize the client with some of its members default values.
  client->socketfd = socketfd;
  client->indices_count = indices_count;
  client->extension_indices = extension_indices;
  client->status = CONNECTED;
  client->current_frame_type = NO_FRAME;
  client->data_frame.type = INVALID;
  client->control_frame.type = INVALID;
  // No control frame will occupy more than 125 bytes
  client->control_frame.buffer_size = CONTROL_FRAME_BUFFER_SIZE;
  client->control_frame.buffer = malloc(CONTROL_FRAME_BUFFER_SIZE);

  // Set up node members and add to table.
  node->client_socketfd = socketfd;
  node->client = client;
  node->next = clients_table[index];
  clients_table[index] = node;
  return client;
}

Client *get_client(int socketfd) {
  // We get the index of the client using the socket descriptor as
  // key. We then search the linked list to get the node containing
  // the client.
  int index = socketfd % HASHTABLE_SIZE;
  Node *node = clients_table[index];
  while (node != NULL) {
    if (node->client_socketfd == socketfd) {
      break;
    }
    node = node->next;
  }

  // If we can't find the client, then we return null.
  if (node == NULL) {
    return NULL;
  }

  return node->client;
}

void __free_client(Client *client) {
  if (client->data_frame.buffer != NULL) {
    free(client->data_frame.buffer);
  }
  if (client->control_frame.buffer != NULL) {
    free(client->control_frame.buffer);
  }
  if (client->output_frame.buffer != NULL) {
    free(client->output_frame.buffer);
  }

  if ( client->indices_count > 0 ) {
    Extension *extension;
    for (uint8_t i = 0; i < client->indices_count; i++) {
        extension = get_extension(client->extension_indices[i]);
        if (extension == NULL) {
          continue;
        }
        extension->close(client->socketfd);
    }
    free(client->extension_indices);
  }

  free(client);
}

void delete_client(Client *client) {
  int index = client->socketfd % HASHTABLE_SIZE;

  Node *prev = clients_table[index];
  Node *current = NULL;
  // Nothing is found in table. That will be strange, but we want to be robust
  if (prev == NULL) {
    __free_client(client);
    return;
  }

  // If client node is at the beginning of the list. Set beginning of list to
  // the next node. Free node and client.
  if (prev->client == client) {
    current = prev;
    clients_table[index] = current->next;
    __free_client(client);
    free(current);
    return;
  }

  // Search for client in list
  current = prev->next;
  while (current != NULL) {
    if (current->client == client) {
      prev->next = current->next;
      break;
    }
    prev = current;
    current = current->next;
  }

  // Free client and its frames
  __free_client(client);
  if (current != NULL) {
    free(current);
  }
}

void print_client(Client *client) {
  printf("Client info\n");
  printf("Socket: %d\n", client->socketfd);
  printf("Status: %d\n", client->status);
  printf("Processing frame: %d\n", (client->current_frame_type != NO_FRAME));
  printf("Header size: %d\n", client->header_size);
  printf("Mask size: %d\n", client->mask_size);
  printf("Mask: %x%x%x%x\n", client->mask[0], client->mask[1], client->mask[2], client->mask[3]);
  printf("Control type: %d\n", client->control_frame.type);
  printf("Control data size: %llu\n", client->control_frame.buffer_size);
  printf("Data type: %d\n", client->data_frame.type);
  printf("Data payload size: %llu\n", client->data_frame.payload_size);
  printf("Current data frame start: %llu\n", client->data_frame.current_fragment_offset);
  printf("Buffer size: %llu\n", client->data_frame.filled_size);
  printf("Buffer max size: %llu\n", client->data_frame.buffer_size);
}
