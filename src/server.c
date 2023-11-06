#include "server.h"

#include <errno.h>
#include <fcntl.h>
#include <openssl/sha.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <unistd.h>

#include "base64.h"
#include "defs.h"
#include "events.h"
#include "frame.h"
#include "header.h"

void handle_connection(int socketfd, bool is_send, bool is_close) {
  Client *client = get_client(socketfd);
  if (client == NULL) {
    // If a client is not in the table, then it is probably initiating the websocket protocol by sending a connection
    // upgrade request. This calls the function that handle the request.
    handle_upgrade(socketfd);
  } else {
    // A client found in the table is more likely sending a frame. This calls the function that handles the frame
    // request.
    if (!is_send && !is_close) {
      handle_client_data(client);
    } else if (is_send) {
      send_frame(client, NULL, -1);
    } else {
      close_client(client);
    }
  }
}

void close_connection(int socketfd) {
  close(socketfd);
  delete_from_event_loop(socketfd);
}

void send_error_response(int socketfd, int status_code, char message[]) {
  char response[256];
  int length;
  char status_405[] = "Method Not Allowed";
  char status_400[] = "Bad Request";
  if (status_code == 405) {
    length = sprintf(response,
                     "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: %lu\r\n\r\n%s",
                     status_code, status_405, strlen(message), message);
  } else {
    length = sprintf(response,
                     "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: %lu\r\n\r\n%s",
                     status_code, status_400, strlen(message), message);
  }
  int sent = send(socketfd, response, length, 0);
  if (sent == -1) {
    perror("send");
  }
  close_connection(socketfd);
}

/**
 * Send upgrade response handshake to a correct client handshake. The response
 * might include a subprotocol or/and extension header if the request contained
 * them.
 *
 * @param socketfd Client socket descriptor
 * @param key Contains the Sec-Websocket-Key value. This function will
 * concatenate its value and the GUID and add them to the buffer.
 * @param subprotocol Contains the first protocol from the request's
 * Sec-Websocket-Protocol values. Can be empty
 * @param subprotocol_len Subprotocol string length. 0 if subprotocol is empty
 * @param extension_indices Array of extension index needed by the client
 * @param indices_count Length of the index array
 *
 * @returns bool true if response is successfully sent, false otherwise
 *
 */
bool __send_upgrade_response(int socketfd, uint8_t key[], char subprotocol[], int subprotocol_len,
                             uint8_t extension_indices[], uint8_t indices_count) {
  uint16_t ext_response_length;
  uint16_t length;
  char response[4096];
  char ext_response[512];
  uint8_t base64[29];
  uint8_t sha1[20];
  // Create `Sec-WebSocket-Accept` value
  strncpy((char *)key + 24, GUID, 36);
  SHA1(key, 60, sha1);
  base64_encode((uint8_t *)sha1, base64, 20);
  base64[28] = '\0';

  // Format response based on the presence of subprotocols
  if (subprotocol_len > 0) {
    length = sprintf(response,
                     "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: "
                     "Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: %s\r\n",
                     base64, subprotocol);
  } else {
    length = sprintf(
        response,
        "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n",
        base64);
  }
  if (indices_count > 0) {
    Extension *extension;
    for (uint8_t i = 0; i < indices_count && length < 4094; i++) {
      extension = get_extension(extension_indices[i]);
      if (extension == NULL) {
        continue;
      }
      ext_response_length = extension->respond_to_offer(socketfd, ext_response);
      if (ext_response_length > 512 || (ext_response_length + length) > 4066) {
        continue;
      }
      length += sprintf(response + length, "Sec-Websocket-Extensions: %s\r\n", ext_response);
    }
  }
  response[length++] = '\r';
  response[length++] = '\n';

  int sent = send(socketfd, response, length, 0);
  if (sent == -1) {
    perror("send");
    close_connection(socketfd);
    return false;
  }
  return true;
}

void handle_upgrade(int socketfd) {
  int16_t nbytes = 0;
  uint16_t total = 0;  // Size Of upgrade request
  char *p;
  char buf[BUFFER_SIZE];  // Buffer that holds request
  uint8_t key[60];        // Buffer that holds sec-websocket-accept value
  char subprotocol[100];
  uint8_t *extension_indices;
  uint8_t indices_count;
  int subprotocol_len;
  while ((nbytes = recv(socketfd, buf + total, BUFFER_SIZE - total, 0)) > 0) {
    total += nbytes;

    // Check for http request validity and break if it's valid
    if (buf[total - 4] == '\r' && buf[total - 3] == '\n' && buf[total - 2] == '\r' && buf[total - 1] == '\n') {
      break;
    }
  }
  subprotocol_len = 0;
  indices_count = 0;
  extension_indices = NULL;

  // Close connection if there's an error or client closes connection.
  if (nbytes <= 0) {
    if (nbytes == 0) {
      // Connection closed
      printf("Closed connection\n");
    } else {
      perror("recv");
    }
    close_connection(socketfd);
    return;
  }

  // Confirm that request is a GET request.
  p = strstr(buf, "GET");
  if (p == NULL || p != buf) {
    send_error_response(socketfd, 405, "Method not allowed");
    return;
  }

  if (!validate_headers(buf, total, socketfd, key, subprotocol, subprotocol_len, &extension_indices, &indices_count)) {
    return;
  }

  bool sent = __send_upgrade_response(socketfd, key, subprotocol, subprotocol_len, extension_indices, indices_count);
  if (sent == true) {
    init_client(socketfd, extension_indices, indices_count);
    // Make socket non-blocking
    fcntl(socketfd, F_SETFL, O_NONBLOCK);
  }
}

void close_client(Client *client) {
  close_connection(client->socketfd);
  delete_client(client);
}

void handle_client_data(Client *client) {
  int nbytes;
  int read;
  int total_read;
  int to_read_size;
  uint8_t data[BUFFER_SIZE];
  uint8_t *buf;
  buf = data;
  to_read_size = BUFFER_SIZE;

  while ((nbytes = recv(client->socketfd, buf, to_read_size, 0)) > 0) {
    total_read = 0;
    while (total_read != nbytes) {
      read = 0;
      // Mask key is the last info in the frame header and is stored in a
      // character buffer. It's used as a proxy to determine if the frame's
      // header data has been extracted. If the length of that buffer isn't
      // 4, the frame header hasn't been completely extracted.
      if (client->mask_size != 4) {
        read = extract_header_data(client, buf + total_read, nbytes - total_read);
        if (read < 0) {
          close_client(client);
          return;
        }
      }
      total_read += read;
      // Some frames have an empty payload. This ensures we don't ignore them
      if (nbytes == total_read &&
          ((client->mask_size == 4 &&
            ((client->current_frame_type == CONTROL_FRAME && client->control_frame.payload_size > 0) ||
             (client->current_frame_type == DATA_FRAME && client->data_frame.payload_size > 0))) ||
           client->mask_size < 4)) {
        break;
      }

      if (client->current_frame_type == CONTROL_FRAME) {
        read = handle_control_frame(client, buf + total_read, nbytes - total_read);
      } else {
        read = handle_data_frame(client, buf + total_read, nbytes - total_read);
      }
      if (read < 0) {
        close_client(client);
        return;
      }
      total_read += read;
    }

    // We can avoid unnecessary copying by storing data directly in the frame buffer
    if (client->mask_size == 4 && client->current_frame_type == DATA_FRAME &&
        (client->data_frame.buffer_size - client->data_frame.filled_size) > BUFFER_SIZE) {
      buf = client->data_frame.buffer + client->data_frame.filled_size;
      to_read_size = client->data_frame.payload_size -
                     (client->data_frame.filled_size - client->data_frame.current_fragment_offset);
    } else {
      buf = data;
      to_read_size = BUFFER_SIZE;
    }
  }
  // Close connection if there's an error or client closes connection.
  if (nbytes <= 0) {
    if (nbytes == 0) {
      // Connection closed
      printf("Closed connection\n");
    } else if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return;
    } else {
      perror("recv");
    }
    close_client(client);
    return;
  }
}

bool send_frame(Client *client, uint8_t *frame, uint64_t size) {
  if (frame == NULL && client->send_buffer == NULL) {
    return true;
  }
  ssize_t total_size;
  ssize_t bytes_sent;
  ssize_t total_bytes_sent;
  uint8_t *buf = NULL;
  if (client->send_buffer != NULL) {
    total_bytes_sent = client->send_start;
    total_size = client->send_buffer_size;

    if (frame != NULL) {
      total_size = total_size - total_bytes_sent;
      if (total_bytes_sent == 0) {
        buf = realloc(client->send_buffer, total_size + size);
        if (buf != NULL) {
          client->send_buffer = buf;
          memcpy(client->send_buffer + total_size, frame, size);
          client->send_buffer_size = total_size + size;
        }
      } else {
        buf = malloc(total_size + size);
        if (buf != NULL) {
          memcpy(buf, client->send_buffer + total_bytes_sent, total_size);
          memcpy(buf + total_size, frame, size);
          free(client->send_buffer);
          client->send_buffer = buf;
          client->send_start = total_bytes_sent = 0;
          client->send_buffer_size = total_size + size;
        }
      }
      total_size = client->send_buffer_size;
    }
    buf = client->send_buffer;
  } else {
    buf = frame;
    total_size = size;
    total_bytes_sent = 0;
  }

  while (total_size > total_bytes_sent) {
    bytes_sent = send(client->socketfd, buf + total_bytes_sent, total_size - total_bytes_sent, 0);
    if (bytes_sent == 0) {
      return false;
    }
    if (bytes_sent < 0) {
      break;
    }
    total_bytes_sent += bytes_sent;
  }

  if (total_bytes_sent == total_size) {
    if (client->send_buffer == NULL) {
      return true;
    }
    free(client->send_buffer);
    client->send_buffer = NULL;
    client->send_start = 0;
    client->send_buffer_size = 0;
    set_write_notify(client->socketfd, false);
    return true;
  }

  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    if (client->send_buffer != NULL) {
      client->send_start = total_bytes_sent;
    } else {
      set_write_notify(client->socketfd, true);
      client->send_start = 0;
      client->send_buffer_size = total_size - total_bytes_sent;
      client->send_buffer = malloc(client->send_buffer_size);
      memcpy(client->send_buffer, buf + total_bytes_sent, total_size - total_bytes_sent);
    }
  } else {
    return false;
  }
  return true;
}