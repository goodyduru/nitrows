#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <openssl/sha.h>

#include "base64.h"
#include "defs.h"
#include "events.h"
#include "frame.h"
#include "header.h"
#include "server.h"

void handle_connection(int socketfd) {
    Client* client = get_client(socketfd);
    if ( client == NULL ) {
        // If a client is not in the table, then it is probably initiating the 
        // websocket protocol by sending a connection upgrade request. This 
        // calls the function that handle the request.
        handle_upgrade(socketfd);
    } else {
        // A client found in the table is more likely sending a frame. This 
        // calls the function that handles the frame request.
        handle_client_data(client);
    }
}

void close_connection(int socketfd) {
    close(socketfd);
    delete_from_event_loop(socketfd, -1);
}

void send_error_response(int socketfd, int status_code, char message[]) {
    char response[256];
    int length;
    char status_405[] = "Method Not Allowed";
    char status_400[] = "Bad Request";
    if ( status_code == 405 ) {
        length = sprintf(response, "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: %lu\r\n\r\n%s", status_code, status_405, strlen(message), message);
    } else {
        length = sprintf(response, "HTTP/1.1 %d %s\r\nConnection: close\r\nContent-Type: text/html\r\nContent-Length: %lu\r\n\r\n%s", status_code, status_400, strlen(message), message);
    }
    int sent = send(socketfd, response, length, 0);
    if ( sent == -1 ) {
        perror("send");
    }
    close_connection(socketfd);
}

void handle_upgrade(int socketfd) {
    int16_t nbytes = 0;
    uint16_t total = 0; // Size Of upgrade request
    char *p;
    char buf[BUFFER_SIZE]; // Buffer that holds request
    uint8_t key[60]; // Buffer that holds sec-websocket-accept value
    char subprotocol[100];
    uint8_t *extension_indices, indices_count;
    int subprotocol_len;
    while ( (nbytes = recv(socketfd, buf+total, BUFFER_SIZE-total, 0)) > 0 ) {
        total += nbytes;

        // Check for http request validity and break if it's valid
        if ( buf[total-4] == '\r' && buf[total-3] == '\n' && buf[total-2] == '\r' && buf[total-1] == '\n' ) {
            break;
        }
    }
    subprotocol_len = 0;
    indices_count = 0;
    extension_indices = NULL;

    // Close connection if there's an error or client closes connection.
    if ( nbytes <= 0 ) {
        if ( nbytes == 0 ) {
            // Connection closed
            printf("Closed connection\n");
        } else {
            perror("recv");
        }
        close_connection(socketfd);
        return;
    }

    // Confirm that request is a GET request.
    if ( (p = strstr(buf, "GET")) == NULL || p != buf ) {
        send_error_response(socketfd, 405, "Method not allowed");
        return;
    }

    if ( !validate_headers(buf, total, socketfd, key, subprotocol,
                                        subprotocol_len, &extension_indices,
                                        &indices_count) ) {
        return;
    }

    bool sent = __send_upgrade_response(socketfd, key, subprotocol,
                                        subprotocol_len, extension_indices,
                                        indices_count);
    if ( sent == true ) {
        init_client(socketfd, extension_indices, indices_count);
    }
}

bool __send_upgrade_response(int socketfd, uint8_t key[], char subprotocol[],
                            int subprotocol_len, uint8_t extension_indices[],
                            uint8_t indices_count) {
    uint16_t ext_response_length, length;
    char response[4096];
    char ext_response[512];
    uint8_t base64[29];
    uint8_t sha1[20];
    // Create `Sec-WebSocket-Accept` value
    strncpy((char*)key+24, GUID, 36);
    SHA1(key, 60, sha1);
    base64_encode((uint8_t *) sha1, base64, 20);
    base64[28] = '\0';

    // Format response based on the presence of subprotocols
    if ( subprotocol_len > 0 ) {
        length = sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: %s\r\n", base64, subprotocol);
    }
    else {
        length = sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n", base64);
    }
    if ( indices_count > 0 ) {
        Extension *extension;
        for ( uint8_t i = 0; i < indices_count && length < 4094; i++ ) {
            extension = get_extension(extension_indices[i]);
            if ( extension == NULL ) {
                continue;
            }
            ext_response_length = extension->respond_to_offer(socketfd, ext_response);
            if ( ext_response_length > 512 ) {
                continue;
            }
            else if ( (ext_response_length + length) > 4066  ) {
                continue;
            }
            length += sprintf(response+length, "Sec-Websocket-Extensions: %s\r\n", ext_response);
        }
    }
    response[length++] = '\r';
    response[length++] = '\n';
    
    int sent = send(socketfd, response, length, 0);
    if ( sent == -1 ) {
        perror("send");
        close_connection(socketfd);
        return false;
    }
    return true;
}

void close_client(Client* client) {
    close_connection(client->socketfd);
    delete_client(client);
}

void handle_client_data(Client* client) {
    int nbytes, read, total_read, to_read_size;
    uint8_t data[BUFFER_SIZE], *buf;
    buf = data;
    to_read_size = BUFFER_SIZE;

    while ( (nbytes = recv(client->socketfd, buf, to_read_size, 0)) > 0 ) {
        total_read = 0;
        while ( total_read != nbytes ) {
            read = 0;
            // Mask key is the last info in the frame header and is stored in a
            // character buffer. It's used as a proxy to determine if the frame's
            // header data has been extracted. If the length of that buffer isn't
            // 4, the frame header hasn't been completely extracted.
            if ( client->mask_size != 4 ) {
                read = extract_header_data(client, buf + total_read,
                                            nbytes - total_read);
                if ( read < 0 ) {
                    close_client(client);
                    return;
                }
            }
            total_read += read;
            // Some frames have an empty payload. This ensures we don't ignore them
            if ( nbytes == total_read ) {
                if ( client->mask_size == 4 && 
                    ((client->current_frame_type == CONTROL_FRAME &&
                    client->control_frame.payload_size > 0) || 
                    (client->current_frame_type == DATA_FRAME &&
                    client->data_frame.payload_size > 0))
                    )  {
                    break;
                } else if ( client->mask_size < 4 ) {
                    break;
                }
            }

            if ( client->current_frame_type == CONTROL_FRAME ) {
                read = handle_control_frame(client, buf + total_read,
                                            nbytes - total_read);
            }
            else {
                read = handle_data_frame(client, buf + total_read,
                                        nbytes - total_read);
            }
            if ( read < 0 ) {
                close_client(client);
                return;
            }
            total_read += read;
            
        }

        // We can avoid unnecessary copying by storing data directly in the frame buffer
        if ( client->mask_size == 4 && client->current_frame_type == DATA_FRAME && (client->data_frame.buffer_size - client->data_frame.filled_size) > BUFFER_SIZE ) {
            buf = client->data_frame.buffer+client->data_frame.filled_size;
            to_read_size = client->data_frame.payload_size - (client->data_frame.filled_size - client->data_frame.current_fragment_offset);
        }
        else {
            buf = data;
            to_read_size = BUFFER_SIZE;
        }
    }
    // Close connection if there's an error or client closes connection.
    if ( nbytes <= 0 ) {
        if ( nbytes == 0 ) {
            // Connection closed
            printf("Closed connection\n");
        } else {
            perror("recv");
        }
        close_client(client);
        return;
    }
}

bool send_frame(Client *client, uint8_t *frame, uint64_t size) {
    ssize_t bytes_sent, total_bytes_sent;
    total_bytes_sent = 0;
    while ( (bytes_sent = send(client->socketfd, frame+total_bytes_sent, size - total_bytes_sent, 0)) > 0 ) {
        total_bytes_sent += bytes_sent;
    }

    return ( bytes_sent == 0 );
}