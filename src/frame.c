#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "frame.h"
#include "utf8.h"
#include "server.h"

int8_t extract_header_data(Client *client, unsigned char buf[], int size) {
    int8_t header_read, read;
    read = 0;
    header_read = 0;
    // If we aren't in frame, extract fin, opcode and check rsvs
    if ( !client->in_frame ) { 
        if ( !get_frame_type(client, buf[0]) ) {
            return -1;
        }
        header_read += 1;
        
        if ( header_read == size ) {
            return header_read;
        }
    }

    // Extract payload size if we haven't
    if ( client->header_size == 0 || (client->payload_size <= 127 && client->current_header[0] != client->payload_size) ) {
        read = get_payload_data(client, buf+header_read, size-header_read);
        if ( read < 0 ) {
            return -1;
        }
        header_read += read;
    }

    //Extract mask
    while ( header_read < size && client->mask_size < 4 ) {
        client->mask[client->mask_size] = buf[header_read];
        client->mask_size++;
        header_read++;
    }
    return header_read;
}

bool get_frame_type(Client *client, unsigned char byte) {
    bool is_final_frame = ( byte > 127 );
    bool valid_rsv_bits = are_rsv_bits_valid(client, byte);

    // If our rsvs are invalid, then send a close frame and -1
    if ( valid_rsv_bits == false ) {
        send_close_status(client, PROTOCOL_ERROR);
        return false;
    } 

    uint8_t opcode = byte & 15;
    // Eliminate invalid opcode
    if ( (opcode > BINARY && opcode < CLOSE) || (opcode > PONG) ) {
        send_close_status(client, PROTOCOL_ERROR);
        return false;
    }

    // We need to set the client struct members that we can set.
    client->is_final_frame = is_final_frame;
    client->is_control_frame = (opcode > THRESHOLD);
    if ( client->is_control_frame ) {
        client->control_type = opcode;
    }
    else {
        if ( !client->is_control_frame && opcode != CONTINUATION && client->data_type == INVALID ) {
            client->data_type = opcode;
        } else if ( opcode == CONTINUATION && client->data_type == INVALID ) {
            send_close_status(client, PROTOCOL_ERROR);
            return false;
        } else if ( opcode != CONTINUATION && client->data_type != INVALID) {
            send_close_status(client, PROTOCOL_ERROR);
            return false;
        }
    }

    if ( !client->is_final_frame && client->is_control_frame ) {
        // A control frame cannot be fragmented
        return false;
    }

    client->in_frame = true;
    return true;
}

bool are_rsv_bits_valid(Client *client, unsigned char byte_data) {
    uint8_t rsv1 = (byte_data & 64) >> 6;
    uint8_t rsv2 = (byte_data & 32) >> 5;
    uint8_t rsv3 = (byte_data & 16) >> 4;

    return (rsv1 == 0 && rsv2 == 0 && rsv3 == 0);
}

int8_t get_payload_data(Client *client, unsigned char buf[], int size) {
    int8_t read = 0;
    if ( client->header_size == 0 ) {
        // Empty header size means we've not check the existence of mask.
        uint8_t has_mask = buf[0] >> 7;
        // Reject lack of mask
        if ( !has_mask ) {
            send_close_status(client, INVALID_TYPE);
            return -1;
        }
    }

    uint8_t header_size, payload_length;
    if ( client->header_size == 0 ) {
        payload_length = (buf[0] & 127);
        client->current_header[0] = payload_length;
        client->header_size = 1;
        header_size = client->header_size;
        read = 1;
    }
    else {
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
    if ( payload_length < 126 ) {
        client->payload_size = payload_length;
        read = 1;
    } else if ( payload_length == 126 && !client->is_control_frame ) {
        // Payload length is a short integer
        while ( header_size < 3 && read < size ) {
            client->current_header[header_size] = buf[read];
            header_size++;
            read++;
        }
        client->header_size = header_size;
        if( header_size < 3 ) {
            // Payload length data is incomplete
            return read;
        }

        uint16_t s;
        memcpy(&s, client->current_header+1, 2);
        client->payload_size = (uint64_t)ntohs(s);
    } else if ( payload_length == 127 && !client->is_control_frame ) {
        // Payload length is a long integer
        while ( header_size < 9 && read < size ) {
            client->current_header[header_size] = buf[read];
            header_size++;
            read++;
        }
        client->header_size = header_size;
        if ( header_size < 9 ) {
            // Payload length data is incomplete
            return read;
        }
        uint64_t s;
        memcpy(&s, client->current_header+1, 8);
        client->payload_size = ntohll(s);
    }
    else {
        // Client is a control frame and its payload is too large
        send_close_status(client, PROTOCOL_ERROR);
        return -1;
    }

    // Check payload size is below our max size and return an error if it is
    if ( client->payload_size > MAX_PAYLOAD_SIZE ) {
        send_close_status(client, TOO_LARGE);
        return -1;
    }
    return read;
}

/**
 * Unmask data gotten from client with mask key.
 */
void unmask(Client *client, unsigned char buf[], int size) {
    for ( int i = 0; i < size; i++ ) {
        buf[i] ^= client->mask[i%4];
    }
}

int8_t handle_control_frame(Client *client, unsigned char buf[], int size) {
    int8_t read = 0;
    unsigned char *data;

    // Avoid unnecessary copies and allocations. We can use the buf directly.
    // If size of buffer is less than payload size, the data needs to be copied
    // to an internal buffer. Also, if something has been copied before, then
    // new data has to be appended to it before processing it.
    if ( client->control_data_size == 0 && size >= client->payload_size ) {
        data = buf;
        read += client->payload_size;
    } 
    else {
        if ( client->control_data == NULL ) {
            client->control_data = (unsigned char *)malloc(client->payload_size);
        }
        data = client->control_data;
        uint8_t to_copy_size = client->payload_size - client->control_data_size;

        if ( to_copy_size >= size ) {
            memcpy(data+client->control_data_size, buf, size);
            client->control_data_size += size;
            read += size;
        }
        else {
            memcpy(data+client->control_data_size, buf, to_copy_size);
            client->control_data_size += to_copy_size;
            read += to_copy_size;
        }
    }

    // Incomplete data, return
    if ( data != buf && client->control_data_size < client->payload_size ) {
        return read;
    }

    unmask(client, data, client->payload_size);
    if ( client->control_type == CLOSE ) {
        if ( client->payload_size > 0 ) {
            uint16_t status_code;
            memcpy(&status_code, data, 2);
            status_code = ntohs(status_code);
            printf("Received close frame with status %d\n", status_code);
            if ( status_code == AWAY ) {
                status_code = htons(1000);
                memcpy(data, &status_code, 2);
            }
        }
        send_close_frame(client, data, client->payload_size);
        return -1;
    } else if ( client->control_type == PING ) {
        bool is_sent = send_pong_frame(client, data, client->payload_size);
        if ( !is_sent ) return -1;
    } else if ( client->control_type == PONG ) {
        printf("Received pong frame");
    }
    // Reset all info
    client->is_control_frame = false;
    client->in_frame = false;
    client->header_size = 0;
    client->payload_size = 0;
    client->mask_size = 0;
    client->control_type = INVALID;
    if ( client->control_data_size > 0 ) {
        free(client->control_data);
        client->control_data = NULL;
        client->control_data_size = 0;
    }
    return read;
}

int64_t handle_data_frame(Client *client, unsigned char buf[], int size) {
    assert(client->buffer_size <= client->buffer_max_size);
    int64_t read = 0;
    uint64_t total_size;
    unsigned char *data;

    // Avoid unnecessary copies and allocations. We can use the buf directly.
    // If size of buffer is less than payload size, the data needs to be copied
    // to an internal buffer. Also, if something has been copied before, then
    // new data has to be appended to it before processing it. This can only 
    // happen if we are processing a final frame.
    if ( client->buffer_size == 0 && size >= client->payload_size &&
         client->is_final_frame ) {
        data = buf;
        read += client->payload_size;
        total_size = client->payload_size;
    } 
    else {

        // Allocate or increase size if we don't have enough space
        if ( client->buffer == NULL ) {
            client->buffer = (unsigned char *)malloc(BUFFER_SIZE);
            client->buffer_max_size = BUFFER_SIZE;
        } else if ( (client->buffer_max_size - client->current_data_frame_start) <           
                    client->payload_size ) {
            uint64_t remaining_space = client->payload_size - (client->buffer_max_size - client->current_data_frame_start);
            remaining_space = (remaining_space + BUFFER_SIZE - 1) & ~(BUFFER_SIZE - 1); // Round to a multiple of buffer size
            if ( (client->buffer_max_size + remaining_space) > MAX_PAYLOAD_SIZE ) {
                send_close_status(client, TOO_LARGE);
                return -1;
            }
            client->buffer_max_size += remaining_space;
            client->buffer = (unsigned char *)realloc(client->buffer, client->buffer_max_size);
        }

        data = client->buffer;
        uint64_t to_copy_size = client->payload_size - (client->buffer_size - client->current_data_frame_start);

        if ( to_copy_size >= size ) {
            memcpy(data+client->buffer_size, buf, size);
            client->buffer_size += size;
            read += size;
        }
        else {
            memcpy(data+client->buffer_size, buf, to_copy_size);
            client->buffer_size += to_copy_size;
            read += to_copy_size;
        }
        total_size = client->buffer_size;
    }
    uint64_t current_frame_size = client->buffer_size - client->current_data_frame_start;

    // Unmask data with current mask data if end of the current frame payload
    // has been reached.
    if ( data == buf || current_frame_size == client->payload_size ) {
        unmask(client, data+client->current_data_frame_start,
               client->payload_size);
    }

    // Incomplete data, return
    if ( data != buf && current_frame_size < client->payload_size ) {
        return read;
    }

    if ( client->is_final_frame ) {
        bool is_text = (client->data_type == TEXT);
        if ( is_text ) {
            bool is_valid = validate_utf8((char *)data, total_size);
            if ( !is_valid ) {
                send_close_status(client, INVALID_ENCODING);
                return -1;
            }
        }
        bool is_sent = send_data_frame(client, data, total_size, is_text);
        if ( !is_sent ) return -1;
    }
    else {
        // Reset client struct without freeing buffer because this is a part of other frames.
        client->in_frame = false;
        client->header_size = 0;
        client->payload_size = 0;
        client->mask_size = 0;
        client->is_control_frame = false;
        client->current_data_frame_start = client->buffer_size;
        return read;
    }

    // Reset client struct
    client->in_frame = false;
    client->header_size = 0;
    client->payload_size = 0;
    client->mask_size = 0;
    client->is_control_frame = false;
    client->data_type = INVALID;
    if ( client->buffer != NULL ) {
        free(client->buffer);
        client->current_data_frame_start = 0;
        client->buffer_max_size = 0;
        client->buffer_size = 0;
        client->buffer = NULL;
    }
    return read;
}

void send_close_status(Client *client, Status_code code) {
    unsigned char statuses[2];
    uint16_t c = htons(code);
    memcpy(statuses, &c, 2);
    send_close_frame(client, statuses, 2);
}

void send_close_frame(Client *client, unsigned char *message, uint8_t size) {
    unsigned char frame[size + 2], payload_size;
    unsigned char first_byte;
    first_byte = 128;
    first_byte |= CLOSE;
    payload_size = 0;
    payload_size |= (127 & size);
    memcpy(frame, &first_byte, 1);
    memcpy(frame+1, &payload_size, 1);
    memcpy(frame+2, message, size);
    send_frame(client, frame, size+2);
}

bool send_pong_frame(Client *client, unsigned char *message, uint8_t size) {
    unsigned char frame[size + 2], payload_size;
    unsigned char first_byte;
    first_byte = 128;
    first_byte |= PONG;
    payload_size = 0;
    payload_size |= (127 & size);
    memcpy(frame, &first_byte, 1);
    memcpy(frame+1, &payload_size, 1);
    memcpy(frame+2, message, size);
    return send_frame(client, frame, size+2);
}

bool send_ping_frame(Client *client, unsigned char *message, uint8_t size) {
    unsigned char frame[size + 2], payload_size;
    unsigned char first_byte;
    first_byte = 128;
    first_byte |= PING;
    payload_size = 0;
    payload_size |= (127 & size);
    frame[0] = first_byte;
    frame[1] = payload_size;
    memcpy(frame+2, message, size);
    return send_frame(client, frame, size+2);
}

bool send_data_frame(Client *client, unsigned char *message, uint64_t size,
                    bool is_text) {
    unsigned char payload_size, size_length, *frame;
    unsigned char first_byte;
    first_byte = 128;
    first_byte |= ( is_text ? TEXT : BINARY );
    payload_size = 0;
    size_length = 0;
    if ( size <= 125 ) {
        payload_size |= (127 & size);
    } else if ( size < 65536 ) {
        payload_size |= 126;
        size_length = 2;
    }
    else {
        payload_size |= 127;
        size_length = 8;
    }
    frame = (unsigned char *) malloc(2 + size_length + size);
    frame[0] = first_byte;
    frame[1] = payload_size;
    if ( size_length == 2 ) {
        uint16_t c = htons(size);
        memcpy(frame+2, &c, 2);
    } else if ( size_length == 8 )  {
        uint64_t c = htonll(size);
        memcpy(frame+2, &c, 8);
    }
    memcpy(frame+size_length+2, message, size);
    return send_frame(client, frame, size+size_length+2);
}