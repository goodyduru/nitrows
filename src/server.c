#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/socket.h>

#include "base64.h"
#include "defs.h"
#include "events.h"
#include "frame.h"
#include "header.h"
#include "server.h"
#include "sha1.h"

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
    int nbytes; // Size Of upgrade request
    char *p;
    char buf[BUFFER_SIZE]; // Buffer that holds request
    char key[60]; // Buffer that holds sec-websocket-accept value
    char subprotocol[100];
    uint8_t *extension_indices, indices_count;
    int subprotocol_len;
    bool is_valid;
    nbytes = recv(socketfd, buf, BUFFER_SIZE, 0);
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

    if ( (is_valid = validate_headers(buf, socketfd, key, subprotocol,
                                        subprotocol_len, &extension_indices,
                                        &indices_count)) == false ) {
        return;
    }

    bool sent = __send_upgrade_response(socketfd, key, subprotocol,
                                        subprotocol_len, extension_indices,
                                        indices_count);
    if ( sent == true ) {
        init_client(socketfd, extension_indices, indices_count);
    }
}

bool __send_upgrade_response(int socketfd, char key[], char subprotocol[],
                            int subprotocol_len, uint8_t extension_indices[],
                            uint8_t indices_count) {
    uint16_t ext_response_length, length;
    char response[4096];
    char ext_response[512];
    unsigned char base64[29];
    char sha1[20];
    // Create `Sec-WebSocket-Accept` value
    strncpy(key+24, GUID, 36);
    SHA1(sha1, key, 60);
    base64_encode((unsigned char *) sha1, base64, 20);
    base64[28] = '\0';

    // Format response based on the presence of subprotocols
    if ( subprotocol_len > 0 ) {
        length = sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: %s\r\n", base64, subprotocol);
    }
    else {
        length = sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n", base64);
    }
    if ( indices_count > 0 ) {
        for ( uint8_t i = 0; i < indices_count && length < 4094; i++ ) {
            ext_response_length = extension_table[i].respond_to_offer(socketfd, ext_response);
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
    int nbytes, read, total_read;
    unsigned char buf[BUFFER_SIZE];

    nbytes = recv(client->socketfd, buf, BUFFER_SIZE, 0);
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
            if ( client->mask_size == 4 && client->current_frame->payload_size > 0 ) {
                break;
            } else if ( client->mask_size < 4 ) {
                break;
            }
        }

        if ( client->current_frame->is_control ) {
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
}

bool send_frame(Client *client, unsigned char *frame, uint64_t size) {
    ssize_t bytes_sent, total_bytes_sent;
    total_bytes_sent = 0;
    while ( (bytes_sent = send(client->socketfd, frame+total_bytes_sent, size - total_bytes_sent, 0)) > 0 ) {
        total_bytes_sent += bytes_sent;
    }

    return ( bytes_sent == 0 );
}