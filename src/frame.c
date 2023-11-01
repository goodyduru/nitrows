#include "frame.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "extension.h"
#include "server.h"
#include "utf8.h"

int8_t extract_header_data(Client *client, uint8_t buf[], int size) {
  int8_t header_read = 0;
  int8_t read = 0;
  // If we aren't in frame, extract fin, opcode and check rsvs
  if (client->current_frame_type == NO_FRAME) {
    if (!get_frame_type(client, buf[0])) {
      return -1;
    }
    header_read += 1;

    if (header_read == size) {
      return header_read;
    }
  }

  // Extract payload size if we haven't
  Frame *current_frame = (client->current_frame_type == CONTROL_FRAME) ? &client->control_frame : &client->data_frame;
  if (client->header_size == 0 ||
      (current_frame->payload_size <= MAX_PAYLOAD_VALUE && client->current_header[0] != current_frame->payload_size)) {
    read = get_payload_data(client, buf + header_read, size - header_read);
    if (read < 0) {
      return -1;
    }
    header_read += read;
  }

  // Extract mask
  while (header_read < size && client->mask_size < 4) {
    client->mask[client->mask_size] = buf[header_read];
    client->mask_size++;
    header_read++;
  }
  return header_read;
}

bool get_frame_type(Client *client, uint8_t byte) {
  bool is_final_frame = (byte > CHAR_MAX);
  bool rsv1 = (byte & 64) >> 6;
  bool rsv2 = (byte & 32) >> 5;
  bool rsv3 = (byte & 16) >> 4;
  bool valid_rsv_bits = true;
  if (client->indices_count == 0) {
    valid_rsv_bits = are_rsv_bits_valid(rsv1, rsv2, rsv3);
  }

  // If our rsvs are invalid, then send a close frame and -1
  if (!valid_rsv_bits) {
    send_close_status(client, PROTOCOL_ERROR);
    return false;
  }

  uint8_t opcode = byte & 15;
  // Eliminate invalid opcode
  if ((opcode > BINARY && opcode < CLOSE) || (opcode > PONG)) {
    send_close_status(client, PROTOCOL_ERROR);
    return false;
  }

  bool is_control = (opcode > THRESHOLD);
  Frame *frame = is_control ? &client->control_frame : &client->data_frame;

  if (is_control) {
    if (!is_final_frame) {
      return false;
    }
    frame->is_first = true;
    client->current_frame_type = CONTROL_FRAME;
  } else {
    if (opcode != CONTINUATION && frame->type == INVALID) {
      frame->is_first = true;
    } else if ((opcode == CONTINUATION && frame->type == INVALID) ||
               (opcode != CONTINUATION && frame->type != INVALID)) {
      send_close_status(client, PROTOCOL_ERROR);
      return false;
    }
    client->current_frame_type = DATA_FRAME;
  }
  frame->is_final = is_final_frame;
  if (frame->type == INVALID) {
    frame->rsv1 = rsv1;
    frame->rsv2 = rsv2;
    frame->rsv3 = rsv3;
    frame->type = opcode;
  }
  return true;
}

bool are_rsv_bits_valid(bool rsv1, bool rsv2, bool rsv3) { return (!rsv1 && !rsv2 && !rsv3); }

int8_t get_payload_data(Client *client, const uint8_t buf[], int size) {
  int8_t read = 0;
  if (client->header_size == 0) {
    // Empty header size means we've not check the existence of mask.
    uint8_t has_mask = buf[0] >> 7;
    // Reject lack of mask
    if (!has_mask) {
      send_close_status(client, INVALID_TYPE);
      return -1;
    }
  }

  uint8_t header_size = 0;
  uint8_t payload_length = 0;
  if (client->header_size == 0) {
    payload_length = (buf[0] & MAX_PAYLOAD_VALUE);
    client->current_header[0] = payload_length;
    client->header_size = 1;
    header_size = client->header_size;
    read = 1;
  } else {
    payload_length = client->current_header[0];
    header_size = client->header_size;
  }

  /**
   * This extracts the payload size and set the client struct payload_size
   * member to the correct value. A payload can be 1 byte, 2 bytes or 8 bytes
   * based on the frame's payload length info. These conditions handle the
   * different sizes. It's possible for the buffer to contain incomplete
   * payload length data. Complete or incomplete, the frame's payload length
   * info is stored in the `current_header` buffer along with the size read
   * (stored in the client's `header_size` member). This is so that it can be
   * completed on receiving the next buffer, if it is incomplete. Only data
   * frames can have payloads that are greater than 125. This is why the
   * extra check for frame type is made. Multibyte length are encoded in
   * network byte order and needs to be converted to host byte order.
   */
  bool is_control = (client->current_frame_type == CONTROL_FRAME);
  Frame *frame = is_control ? &client->control_frame : &client->data_frame;
  if (payload_length < (MAX_PAYLOAD_VALUE - 1)) {
    frame->payload_size = payload_length;
    read = 1;
  } else if (payload_length == (MAX_PAYLOAD_VALUE - 1) && !is_control) {
    // Payload length is a short integer
    while (header_size < 3 && read < size) {
      client->current_header[header_size] = buf[read];
      header_size++;
      read++;
    }
    client->header_size = header_size;
    if (header_size < 3) {
      // Payload length data is incomplete
      return read;
    }

    uint16_t s = 0;
    memcpy(&s, client->current_header + 1, 2);
    frame->payload_size = (uint64_t)ntohs(s);
  } else if (payload_length == MAX_PAYLOAD_VALUE && !is_control) {
    // Payload length is a long integer
    while (header_size < MAX_FRAME_HEADER_SIZE && read < size) {
      client->current_header[header_size] = buf[read];
      header_size++;
      read++;
    }
    client->header_size = header_size;
    if (header_size < MAX_FRAME_HEADER_SIZE) {
      // Payload length data is incomplete
      return read;
    }
    uint64_t s = 0;
    memcpy(&s, client->current_header + 1, 8);
    frame->payload_size = ntohll(s);
  } else {
    // Client is a control frame and its payload is too large
    send_close_status(client, PROTOCOL_ERROR);
    return -1;
  }

  // Check payload size is below our max size and return an error if it is
  if (frame->payload_size > MAX_PAYLOAD_SIZE) {
    send_close_status(client, TOO_LARGE);
    return -1;
  }
  return read;
}

/**
 * Unmask data gotten from client with mask key.
 */
void unmask(Client *client, uint8_t buf[], int size) {
  for (int i = 0; i < size; i++) {
    buf[i] ^= client->mask[i % 4];
  }
}

/**
 * This processes and responds to a close frame from the client. The spec
 * defines various ways of responding to a close request. Here it responds
 * based on the size of the payload. Most of the time, it tries to echo back
 * the same valid status code that's processed. Empty close frame are sent for
 * invalid code.
 */
void handle_close_frame(Client *client, uint8_t *data) {
  uint64_t payload_size = client->control_frame.payload_size;
  if (payload_size == 0) {
    send_close_status(client, NORMAL);
    return;
  }

  if (payload_size == 1) {
    send_close_status(client, PROTOCOL_ERROR);
    return;
  }
  uint16_t status_code = 0;
  memcpy(&status_code, data, 2);
  status_code = ntohs(status_code);
  printf("Received close frame with status %d\n", status_code);
  int16_t new_status_code = get_reply_code(status_code);
  // -1 implies an invalid status code, more than 0 means a new code is
  // sent. 0 means that the same code should be echoed back. Only greater
  // than 0 requires a copy, since an echo means the code is already in
  // the data.
  if (new_status_code == -1) {
    send_close_status(client, 0);
    return;
  }
  if (new_status_code > 0) {
    new_status_code = htons(new_status_code);
    memcpy(data, &new_status_code, 2);
  }

  // Validate utf-8
  bool is_valid = validate_utf8((char *)data + 2, payload_size - 2);
  if (!is_valid) {
    send_close_status(client, INVALID_ENCODING);
    return;
  }
  send_close_frame(client, data, payload_size);
}

int16_t get_reply_code(uint16_t status_code) {
  int16_t reply = 0;
  /**
   * We return normal if code is away, for defined and valid codes, we return
   * 0 to avoid unnecessary copies. For others, we return -1 to send an empty
   * close frame.
   */
  switch (status_code) {
    case AWAY:
      reply = NORMAL;
      break;
    case NORMAL:
    case PROTOCOL_ERROR:
    case INVALID_TYPE:
    case INVALID_ENCODING:
    case VIOLATION:
    case TOO_LARGE:
    case INVALID_EXTENSION:
    case UNEXPECTED_CONDITION:
      reply = 0;
      break;
    default:
      reply = -1;
  }
  return reply;
}

int8_t handle_control_frame(Client *client, uint8_t buf[], int size) {
  int8_t read = 0;
  uint8_t *data = NULL;
  Frame *frame = &client->control_frame;

  // Avoid unnecessary copies and allocations. We can use the buf directly.
  // If size of buffer is less than payload size, the data needs to be copied
  // to an internal buffer. Also, if something has been copied before, then
  // new data has to be appended to it before processing it.
  if (frame->filled_size == 0 && size >= frame->payload_size) {
    data = buf;
    read += frame->payload_size;
  } else {
    data = frame->buffer;
    uint8_t to_copy_size = frame->payload_size - frame->filled_size;
    to_copy_size = (to_copy_size >= size) ? size : to_copy_size;
    memcpy(data + frame->filled_size, buf, to_copy_size);
    frame->filled_size += to_copy_size;
    read += to_copy_size;
  }

  // Incomplete data, return
  if (data != buf && frame->filled_size < frame->payload_size) {
    return read;
  }

  unmask(client, data, frame->payload_size);
  if (frame->type == CLOSE) {
    handle_close_frame(client, data);
    return -1;
  }

  if (frame->type == PING) {
    bool is_sent = send_pong_frame(client, data, frame->payload_size);
    if (!is_sent) {
      return -1;
    }
  } else if (frame->type == PONG) {
    printf("Received pong frame");
  }
  // Reset all info
  frame->filled_size = 0;
  frame->is_final = false;
  frame->is_first = false;
  frame->payload_size = 0;
  frame->type = INVALID;
  client->header_size = 0;
  client->mask_size = 0;
  client->current_frame_type = NO_FRAME;
  return read;
}

int64_t handle_data_frame(Client *client, uint8_t buf[], int size) {
  int64_t read = 0;
  Frame *frame = &client->data_frame;
  uint8_t *data = frame->buffer;
  uint8_t *temp;

  // Avoid unnecessary copies and allocations. We can use the buf directly.
  // If size of buffer is less than payload size, the data needs to be copied
  // to an internal buffer. Also, if something has been copied before, then
  // new data has to be appended to it before processing it. This can only
  // happen if we are processing a final frame.
  if (frame->filled_size == 0 && size >= frame->payload_size && frame->is_final && frame->is_first) {
    data = buf;
    read += frame->payload_size;
  } else if (buf >= frame->buffer && buf < frame->buffer + frame->buffer_size) {
    // If buf is part of the frame data frame buffer, we just need to increase filled size attribute
    frame->filled_size += size;
    read = size;
  } else if (frame->payload_size > 0) {
    // Allocate or increase size if we don't have enough space
    if (frame->buffer == NULL) {
      frame->buffer = (uint8_t *)malloc(frame->payload_size);
      frame->buffer_size = frame->payload_size;
      data = frame->buffer;
    } else if ((frame->buffer_size - frame->current_fragment_offset) < frame->payload_size) {
      uint64_t remaining_space = frame->payload_size - (frame->buffer_size - frame->current_fragment_offset);
      if ((frame->buffer_size + remaining_space) > MAX_PAYLOAD_SIZE) {
        send_close_status(client, TOO_LARGE);
        return -1;
      }
      frame->buffer_size += remaining_space;
      frame->buffer_size =
          (frame->buffer_size + BUFFER_SIZE - 1) & ~(BUFFER_SIZE - 1);  // Round to a multiple of buffer size
      temp = (uint8_t *)realloc(frame->buffer, frame->buffer_size);
      if (temp == NULL) {
        send_close_status(client, TOO_LARGE);
        return -1;
      }
      frame->buffer = temp;
      data = frame->buffer;
    }
    uint64_t to_copy_size = frame->payload_size - (frame->filled_size - frame->current_fragment_offset);
    to_copy_size = (to_copy_size >= size) ? size : to_copy_size;

    memcpy(data + frame->filled_size, buf, to_copy_size);
    frame->filled_size += to_copy_size;
    read += to_copy_size;
  }
  uint64_t current_frame_size = frame->filled_size - frame->current_fragment_offset;
  // Unmask data with current mask data if end of the current frame payload
  // has been reached.
  if (data == buf || current_frame_size == frame->payload_size) {
    unmask(client, data + frame->current_fragment_offset, frame->payload_size);
  }

  // Incomplete data, return
  if (data != buf && current_frame_size < frame->payload_size) {
    return read;
  }

  if (frame->is_final) {
    bool is_valid = false;
    bool was_written = false;
    if (frame->type == TEXT && client->indices_count == 0) {
      if (data == buf) {
        is_valid = validate_utf8((char *)data, frame->payload_size);
      } else {
        is_valid = validate_utf8((char *)frame->buffer, frame->filled_size);
      }
      if (!is_valid) {
        send_close_status(client, INVALID_ENCODING);
        return -1;
      }
    } else if (client->indices_count > 0) {
      uint8_t *output = NULL;
      uint64_t output_length = 0;
      Extension *extension = NULL;
      if (data == buf) {
        frame->buffer = data;
        frame->buffer_size = frame->payload_size;
        frame->filled_size = frame->payload_size;
      }
      for (uint8_t i = 0; i < client->indices_count; i++) {
        extension = get_extension(client->extension_indices[i]);
        if (extension == NULL) {
          continue;
        }
        is_valid = extension->process_data(client->socketfd, frame, &output, &output_length);
        if (!is_valid) {
          send_close_status(client, INVALID_EXTENSION);
          return -1;
        }
        if (output_length > 0) {
          if (frame->buffer != buf && frame->buffer != NULL) {
            free(frame->buffer);
          }
          was_written = true;
          frame->buffer = output;
          frame->payload_size = output_length;
          frame->buffer_size = output_length;
          frame->filled_size = output_length;
          output = NULL;
          output_length = 0;
        }
      }
    }
    if (was_written) {
      data = frame->buffer;
    }
    bool is_sent = send_data_frame(client, data);
    if (!is_sent) {
      return -1;
    }
  } else {
    // Reset client struct without freeing buffer because this is a part of other frames.
    frame->current_fragment_offset = frame->filled_size;
    client->header_size = 0;
    client->mask_size = 0;
    client->current_frame_type = NO_FRAME;
    frame->payload_size = 0;
    return read;
  }

  // Reset client struct
  client->header_size = 0;
  client->mask_size = 0;
  client->current_frame_type = NO_FRAME;
  frame->is_final = false;
  frame->is_first = false;
  frame->payload_size = 0;
  frame->type = INVALID;
  frame->current_fragment_offset = 0;
  if (frame->buffer_size > 0 && data != buf) {
    frame->buffer_size = 0;
    frame->filled_size = 0;
    free(frame->buffer);
    frame->buffer = NULL;
  }
  return read;
}

void send_close_status(Client *client, Status_code code) {
  uint8_t statuses[2];
  if (code > EMPTY_FRAME) {
    uint16_t c = htons(code);
    memcpy(statuses, &c, 2);
  }
  send_close_frame(client, statuses, (code == EMPTY_FRAME) ? 0 : 2);
}

void send_close_frame(Client *client, uint8_t *message, uint8_t size) {
  uint8_t frame[size + 2];
  uint8_t payload_size = (MAX_PAYLOAD_VALUE & size);
  uint8_t first_byte = 128;
  first_byte |= CLOSE;
  memcpy(frame, &first_byte, 1);
  memcpy(frame + 1, &payload_size, 1);
  memcpy(frame + 2, message, size);
  send_frame(client, frame, size + 2);
}

bool send_pong_frame(Client *client, uint8_t *message, uint8_t size) {
  uint8_t frame[size + 2];
  uint8_t payload_size = (MAX_PAYLOAD_VALUE & size);
  uint8_t first_byte = 128;
  first_byte |= PONG;
  memcpy(frame, &first_byte, 1);
  memcpy(frame + 1, &payload_size, 1);
  memcpy(frame + 2, message, size);
  return send_frame(client, frame, size + 2);
}

bool send_ping_frame(Client *client, uint8_t *message, uint8_t size) {
  uint8_t frame[size + 2];
  uint8_t payload_size = (MAX_PAYLOAD_VALUE & size);
  uint8_t first_byte = 128;
  first_byte |= PING;
  frame[0] = first_byte;
  frame[1] = payload_size;
  memcpy(frame + 2, message, size);
  return send_frame(client, frame, size + 2);
}

bool send_data_frame(Client *client, uint8_t *message) {
  uint8_t payload_size = 0;
  uint8_t size_length = 0;
  uint8_t first_byte = 128;
  uint8_t *final_frame = NULL;
  uint64_t size = 0;
  uint64_t output_size = 0;
  Frame *frame = &client->data_frame;
  Extension *extension;
  if (frame->buffer_size == 0) {
    size = frame->payload_size;
  } else {
    size = frame->filled_size;
  }
  for (uint8_t i = 0; i < client->indices_count; i++) {
    extension = get_extension(client->extension_indices[i]);
    if (extension == NULL) {
      continue;
    }
    output_size = extension->generate_data(client->socketfd, message, size, &client->output_frame);
    if (output_size > 0) {
      if (i > 0) {
        free(message);
      }
      message = client->output_frame.buffer;
      size = output_size;
    }
    if (output_size == 0 && size > 0) {
      send_close_status(client, INVALID_EXTENSION);
      return -1;
    }
    first_byte |= (client->output_frame.rsv1 << 6);
    first_byte |= (client->output_frame.rsv2 << 5);
    first_byte |= (client->output_frame.rsv3 << 4);
  }
  first_byte |= frame->type;
  if (size <= (MAX_PAYLOAD_VALUE - 2)) {
    payload_size |= (MAX_PAYLOAD_VALUE & size);
  } else if (size <= UINT16_MAX) {
    payload_size |= (MAX_PAYLOAD_VALUE - 1);
    size_length = 2;
  } else {
    payload_size |= MAX_PAYLOAD_VALUE;
    size_length = 8;
  }
  final_frame = (uint8_t *)malloc(2 + size_length + size);
  final_frame[0] = first_byte;
  final_frame[1] = payload_size;
  if (size_length == 2) {
    uint16_t c = htons(size);
    memcpy(final_frame + 2, &c, 2);
  } else if (size_length == 8) {
    uint64_t c = htonll(size);
    memcpy(final_frame + 2, &c, 8);
  }
  memcpy(final_frame + size_length + 2, message, size);
  bool is_sent = send_frame(client, final_frame, size + size_length + 2);
  free(final_frame);
  if (client->output_frame.buffer != NULL) {
    free(client->output_frame.buffer);
    client->output_frame.buffer_size = 0;
    client->output_frame.payload_size = 0;
    client->output_frame.rsv1 = false;
    client->output_frame.buffer = NULL;
  }
  return is_sent;
}