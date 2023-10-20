#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "extension.h"
#include "frame.h"
#include "utf8.h"
#include "server.h"

Frame *get_new_data_frame(Client *client) {
    if ( client->data_frames == NULL ) {
        client->data_frames = (Frame *)calloc(1, sizeof(Frame));
        client->frame_count = 1;
        client->frame_size = 1;
        return client->data_frames;
    }

    if ( client->frame_size == client->frame_count ) {
        client->frame_size *= 2;
        client->data_frames = (Frame *) realloc(client->data_frames, client->frame_size*sizeof(Frame));
    }
    Frame *frame = &client->data_frames[client->frame_count];
    client->frame_count++;

    // Set some default values
    frame->is_first = false;
    frame->payload_size = 0;
    frame->buffer_size = 0;
    frame->buffer = NULL;
    return frame;
}

/**
 * This function is called on reallocation of the shared buffer. Since
 * reallocation changes pointer address, each frame buffer pointer needs to be
 * changed too, since they all point to offsets of the same shared buffer.
 */
void update_data_frames_buffer(Client *client) {
    uint64_t payload = 0;
    for ( int i = 0; i < client->frame_count; i++ ) {
        client->data_frames[i].buffer = client->shared_buffer+payload;
        payload += client->data_frames[i].payload_size;
    }
}

int8_t extract_header_data(Client *client, unsigned char buf[], int size) {
    int8_t header_read, read;
    read = 0;
    header_read = 0;
    // If we aren't in frame, extract fin, opcode and check rsvs
    if ( client->current_frame == NULL ) { 
        if ( !get_frame_type(client, buf[0]) ) {
            return -1;
        }
        header_read += 1;
        
        if ( header_read == size ) {
            return header_read;
        }
    }

    // Extract payload size if we haven't
    if ( client->header_size == 0 || (client->current_frame->payload_size <= 127 && client->current_header[0] != client->current_frame->payload_size) ) {
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
    Frame *frame;
    bool is_final_frame = ( byte > 127 );
    bool rsv1 = (byte & 64) >> 6;
    bool rsv2 = (byte & 32) >> 5;
    bool rsv3 = (byte & 16) >> 4;
    bool valid_rsv_bits = true;
    if ( client->indices_count == 0 ) {
        valid_rsv_bits = are_rsv_bits_valid(rsv1, rsv2, rsv3);
    }

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


    bool is_control = (opcode > THRESHOLD);
    if ( is_control ) {
        client->current_frame = (Frame *)calloc(1, sizeof(Frame));
    }
    else {
        client->current_frame = get_new_data_frame(client);
    }
    frame = client->current_frame;
    frame->is_final = is_final_frame;
    frame->rsv1 = rsv1;
    frame->rsv2 = rsv2;
    frame->rsv3 = rsv3;
    frame->is_control = is_control;
    frame->type = opcode;

    if ( frame->is_control || (opcode != CONTINUATION) ) {
        frame->is_first = true;
    }

    if ( !frame->is_first && client->frame_count == 1 ) {
        send_close_status(client, PROTOCOL_ERROR);
        return false;
    } else if ( frame->is_first && !frame->is_control && client->frame_count > 1 ) {
        send_close_status(client, PROTOCOL_ERROR);
        return false;
    }

    if ( !frame->is_final && frame->is_control ) {
        // A control frame cannot be fragmented
        return false;
    }
    return true;
}

bool are_rsv_bits_valid(bool rsv1, bool rsv2, bool rsv3) {
    return (!rsv1 && !rsv2 && !rsv3);
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
    Frame *frame = client->current_frame;
    if ( payload_length < 126 ) {
        frame->payload_size = payload_length;
        read = 1;
    } else if ( payload_length == 126 && !frame->is_control ) {
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
        frame->payload_size = (uint64_t)ntohs(s);
    } else if ( payload_length == 127 && !frame->is_control ) {
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
        frame->payload_size = ntohll(s);
    }
    else {
        // Client is a control frame and its payload is too large
        send_close_status(client, PROTOCOL_ERROR);
        return -1;
    }

    // Check payload size is below our max size and return an error if it is
    if ( frame->payload_size > MAX_PAYLOAD_SIZE ) {
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

/**
 * This processes and responds to a close frame from the client. The spec
 * defines various ways of responding to a close request. Here it responds
 * based on the size of the payload. Most of the time, it tries to echo back
 * the same valid status code that's processed. Empty close frame are sent for
 * invalid code.
 */
void handle_close_frame(Client *client, unsigned char *data) {
    Frame *frame = client->current_frame;
    if ( frame->payload_size == 0 ) {
        send_close_status(client, NORMAL);
        return;
    } else if ( frame->payload_size == 1 ) {
        send_close_status(client, PROTOCOL_ERROR);
        return;
    }
    else {
        uint16_t status_code;
        memcpy(&status_code, data, 2);
        status_code = ntohs(status_code);
        printf("Received close frame with status %d\n", status_code);
        int16_t new_status_code = get_reply_code(status_code);
        // -1 implies an invalid status code, more than 0 means a new code is
        // sent. 0 means that the same code should be echoed back. Only greater
        // than 0 requires a copy, since an echo means the code is already in
        // the data.
        if ( new_status_code == -1 ) {
            send_close_status(client, 0);
            return;
        } else if ( new_status_code > 0 ) {
            memcpy(data, &new_status_code, 2);
        }

        // Validate utf-8
        bool is_valid = validate_utf8((char *)data+2, frame->payload_size-2);
        if ( !is_valid ) {
            send_close_status(client, INVALID_ENCODING);
            return;
        }
    }
    send_close_frame(client, data, frame->payload_size);
}

int8_t handle_control_frame(Client *client, unsigned char buf[], int size) {
    int8_t read = 0;
    unsigned char *data;
    Frame *frame = client->current_frame;

    // Avoid unnecessary copies and allocations. We can use the buf directly.
    // If size of buffer is less than payload size, the data needs to be copied
    // to an internal buffer. Also, if something has been copied before, then
    // new data has to be appended to it before processing it.
    if ( frame->buffer_size == 0 && size >= frame->payload_size ) {
        data = buf;
        read += frame->payload_size;
    } 
    else {
        if ( frame->buffer == NULL ) {
            frame->buffer = (unsigned char *)malloc(frame->payload_size);
        }
        data = frame->buffer;
        uint8_t to_copy_size = frame->payload_size - frame->buffer_size;

        if ( to_copy_size >= size ) {
            memcpy(data+frame->buffer_size, buf, size);
            frame->buffer_size += size;
            read += size;
        }
        else {
            memcpy(data+frame->buffer_size, buf, to_copy_size);
            frame->buffer_size += to_copy_size;
            read += to_copy_size;
        }
    }

    // Incomplete data, return
    if ( data != buf && frame->buffer_size < frame->payload_size ) {
        return read;
    }

    unmask(client, data, frame->payload_size);
    if ( frame->type == CLOSE ) {
        handle_close_frame(client, data);
        return -1;
    } else if ( frame->type == PING ) {
        bool is_sent = send_pong_frame(client, data, frame->payload_size);
        if ( !is_sent ) return -1;
    } else if ( frame->type == PONG ) {
        printf("Received pong frame");
    }
    // Reset all info
    client->header_size = 0;
    client->mask_size = 0;
    if ( frame->buffer_size > 0 ) {
        free(frame->buffer);
    }
    free(frame);
    client->current_frame = NULL;
    return read;
}

int64_t handle_data_frame(Client *client, unsigned char buf[], int size) {
    int64_t read = 0;
    unsigned char *data;
    Frame *frame = client->current_frame;

    // Avoid unnecessary copies and allocations. We can use the buf directly.
    // If size of buffer is less than payload size, the data needs to be copied
    // to an internal buffer. Also, if something has been copied before, then
    // new data has to be appended to it before processing it. This can only 
    // happen if we are processing a final frame.
    if ( frame->buffer_size == 0 && size >= frame->payload_size &&
         frame->is_final && frame->is_first ) {
        data = buf;
        read += frame->payload_size;
    } 
    else {
        // Allocate or increase size if we don't have enough space
        if ( client->shared_buffer == NULL && frame->payload_size > 0 ) {
            client->shared_buffer = (unsigned char *)malloc(frame->payload_size);
            client->buffer_limit = frame->payload_size;
            frame->buffer = client->shared_buffer;
        }
        else if ( (client->buffer_limit - client->buffer_size) <        
                    frame->payload_size ) {
            uint64_t remaining_space;
            if ( client->buffer_limit > BUFFER_SIZE && frame->payload_size < BUFFER_SIZE ) {
                remaining_space = client->buffer_limit; // Just double
            }
            else {
                remaining_space = frame->payload_size - (client->buffer_limit - client->buffer_size);
            }
            remaining_space = (remaining_space + BUFFER_SIZE - 1) & ~(BUFFER_SIZE - 1); // Round to a multiple of buffer size

            if ( (client->buffer_limit + remaining_space) > MAX_PAYLOAD_SIZE ) {
                send_close_status(client, TOO_LARGE);
                return -1;
            }
            client->buffer_limit += remaining_space;
            client->shared_buffer = (unsigned char *)realloc(client->shared_buffer, client->buffer_limit);
            update_data_frames_buffer(client);
        }

        if ( frame->buffer == NULL ) {
            frame->buffer = client->shared_buffer+client->buffer_size;
        }
        data = frame->buffer;
        uint64_t to_copy_size = frame->payload_size - frame->buffer_size;
        to_copy_size = (to_copy_size >= size) ? size : to_copy_size;

        memcpy(data+frame->buffer_size, buf, to_copy_size);
        frame->buffer_size += to_copy_size;
        client->buffer_size += to_copy_size;
        read += to_copy_size;
    }

    // Unmask data with current mask data if end of the current frame payload
    // has been reached.
    if ( data == buf || frame->buffer_size == frame->payload_size ) {
        unmask(client, data, frame->payload_size);
    }

    // Incomplete data, return
    if ( data != buf && frame->buffer_size < frame->payload_size ) {
        return read;
    }
    if ( data != buf ) {
        assert(frame->buffer_size == frame->payload_size);
    }

    
    if ( frame->is_final ) {
        bool is_valid, was_written = false;
        if ( client->data_frames->type == TEXT && client->indices_count == 0 ) {
            if ( data == buf ) {
                is_valid = validate_utf8((char *)data, frame->payload_size);
            }
            else {
                is_valid = validate_utf8((char *)client->shared_buffer, client->buffer_size);
            }
            if ( !is_valid ) {
                send_close_status(client, INVALID_ENCODING);
                return -1;
            }
        } else if ( client->indices_count > 0 ) {
            uint8_t *output = NULL;
            uint64_t output_length = 0;
            if ( data == buf ) {
                client->data_frames[0].buffer = data;
            }
            for ( uint8_t i = 0; i < client->indices_count; i++ ) {
                is_valid = extension_table[client->extension_indices[i]].process_data(client->socketfd, client->data_frames, client->frame_count, &output, &output_length);
                if ( !is_valid ) {
                    send_close_status(client, INVALID_EXTENSION);
                    return -1;
                }
                if ( output_length > 0 ) {
                    was_written = true;
                    if ( data != buf )
                        free(client->data_frames[0].buffer);
                    client->data_frames[0].buffer = output;
                    client->data_frames[0].payload_size = output_length;
                    client->buffer_size = output_length;
                    client->frame_count = 1;
                    output = NULL;
                    output_length = 0;
                }
            }
        }
        if ( data != buf && !was_written ) {
            data = client->shared_buffer;
        } else if ( was_written ) {
            data = client->data_frames[0].buffer;
        }
        bool is_sent = send_data_frame(client, data);
        if ( !is_sent ) return -1;
    }
    else {
        // Reset client struct without freeing buffer because this is a part of other frames.
        client->header_size = 0;
        client->mask_size = 0;
        client->current_frame = NULL;
        return read;
    }

    // Reset client struct
    client->header_size = 0;
    client->mask_size = 0;
    client->current_frame = NULL;
    client->frame_count = 0;
    client->frame_size = 0;
    free(client->data_frames);
    client->data_frames = NULL;
    if ( client->buffer_limit > 0 ) {
        client->buffer_limit = 0;
        client->buffer_size = 0;
        free(client->shared_buffer);
        client->shared_buffer = NULL;
    }
    return read;
}

int16_t get_reply_code(uint16_t status_code) {
    int16_t reply = 0;
    /**
     * We return normal if code is away, for defined and valid codes, we return
     * 0 to avoid unnecessary copies. For others, we return -1 to send an empty
     * close frame.
    */
    switch ( status_code ) {
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
    return -1;
}

void send_close_status(Client *client, Status_code code) {
    unsigned char statuses[2];
    if ( code > EMPTY_FRAME ) {
        uint16_t c = htons(code);
        memcpy(statuses, &c, 2);
    }
    send_close_frame(client, statuses, (code == EMPTY_FRAME) ? 0 : 2);
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

bool send_data_frame(Client *client, unsigned char *message) {
    unsigned char payload_size, size_length, *frame;
    unsigned char first_byte;
    uint64_t size;
    if ( client->buffer_limit == 0 ) {
        size = client->current_frame->payload_size;
    }
    else {
        size = client->buffer_size;
    }
    first_byte = 128;
    for ( uint8_t i = 0; i < client->indices_count; i++ ) {
        size = extension_table[client->extension_indices[i]].generate_data(client->socketfd, message, size, client->current_frame);
        message = client->current_frame->buffer;
        if ( size == 0 ) {
            send_close_status(client, INVALID_EXTENSION);
            return -1;
        }
        first_byte |= (client->current_frame->rsv1 << 6);
        first_byte |= (client->current_frame->rsv2 << 5);
        first_byte |= (client->current_frame->rsv3 << 4);
    }
    first_byte |= client->data_frames[0].type;
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