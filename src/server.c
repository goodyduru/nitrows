#include <ctype.h>
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

/**
 * Send http error response and close the connection
 * 
 * @param socketfd Client socket descriptor
 * @param status_code HTTP status code
 * @param message Response body
 */
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

/**
 * Check the presence and validity of the `Upgrade` header in the opening
 * request sent by the client. Send an error response if the check fails
 * 
 * @param socketfd Client socket descriptor
 * @param buf String buffer containing client request
 * 
 * @return Validity of the header as a boolean
 */
bool is_upgrade_header_valid(int socketfd, char buf[]) {
    char *p;
    int cmp;

     // Confirm the presence of header
    if ( (p = strcasestr(buf, "Upgrade:")) == NULL ) {
        send_error_response(socketfd, 400, "Upgrade header not included");
        return false;
    }
    // Get the value of the header and confirm that its value is websocket
    p += 8; // Increment pointer by the length of `Upgrade:`

    // Search for the presence of the first non-whitespace character
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' ) {
        send_error_response(socketfd, 400, "Invalid Upgrade header");
        return false;
    }
    if ( (cmp = strncasecmp(p, "websocket", 9)) != 0 ) {
        send_error_response(socketfd, 400, "Invalid Upgrade header value");
        return false;
    }
    return true;
}

/**
 * Check the presence and validity of the `Sec-Websocket-Key` header in the
 * opening request sent by the client. In addition, get the header value and
 * check its validity too. Send an error response if any of the checks fail.
 * Return the header value in the `key` parameter if all the checks succeed.
 * 
 * @param socketfd Client socket descriptor
 * @param buf String buffer containing client request
 * @param key String buffer that will contain the key header value
 * 
 * @return Validity of header as a boolean
 */
bool get_sec_websocket_key_value(int socketfd, char buf[], char key[]) {
    char *p, c;
    // Confirm the presence of header
    if ( (p = strcasestr(buf, "Sec-Websocket-Key:")) == NULL ) {
        send_error_response(socketfd, 400, "Sec-Websocket-Key not included");
        return false;
    }
    // Get the value of the key header
    p += 18; // Increment pointer by the length of `Sec-Websocket-Key`
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' ) {
        send_error_response(socketfd, 400, "Invalid Sec-Websocket-Key header");
        return false;
    }
    int i = 0;
    while ( i < 22 && *p != '\0' ) {
        c = *p;
        // Allow only valid base64 characters for the first 22 characters
        if ( !isalnum(c) && c != '+' && c != '/' ) {
            break;
        }
        key[i] = c;
        i++;
        p++;
    }
    if ( i < 22 ) {
        send_error_response(socketfd, 400, 
                            "Invalid Sec-Websocket-Key header value");
        return false;
    }
    // Valid Sec-Websocket-Key value must end with 2 '=' characters
    if ( *p != '=' || *(p+1) != '=' ) {
        send_error_response(socketfd, 400, 
                            "Invalid Sec-Websocket-Key header value");
        return false;
    }
    key[22] = '=';
    key[23] = '=';
    return true;
}

/**
 * Check the presence and validity of the `Sec-Websocket-Version` header in the
 * opening request sent by the client. Send an error response if the check fails
 * 
 * @param socketfd Client socket descriptor
 * @param buf String buffer containing client request
 * 
 * @return Validity of the header as a boolean
 */
bool is_version_header_valid(int socketfd, char buf[]) {
    char *p;
    int cmp;
     // Confirm the presence of header
    if ( (p = strcasestr(buf, "Sec-Websocket-Version:")) == NULL ) {
        send_error_response(socketfd, 400,
                            "Sec-Websocket-Version header not included");
        return false;
    }
    // Get the value of the header and confirm that its value is 13
    p += 22; // Increment pointer by the length of `Upgrade:`
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' ) {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Version header");
        return false;
    }
    if ( (cmp = strncasecmp(p, "13", 2)) != 0 ) {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Version header value");
        return false;
    }
    return true;
}

/**
 * Check the presence and validity of the `Sec-Websocket-Protocol` header in
 * the opening request sent by the client. This header is optional, so absence
 * of the header is valid. Once there's a value, then it has to have a valid
 * format. Send an error response if the checks fails. We return the first
 * subprotocol and its length in the provided parameters.
 * 
 * @param socketfd Client socket descriptor
 * @param buf String buffer containing client request
 * @param subprotocol This will contain the chosen protocol
 * @param subprotocol_len This will contain the pointer to the chosen protocol
 * length
 * 
 * @return Validity of the header as a boolean
 */
bool get_subprotocols(int socketfd, char buf[], char subprotocol[],
                                int *subprotocol_len) {
    char *p;
    // Confirm the presence of header
    if ( (p = strcasestr(buf, "Sec-Websocket-Protocol:")) == NULL ) {
        return true;
    }
    // Get the value of the key header
    p += 23; // Increment pointer by the length of `Sec-Websocket-Protocol`
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }

    // Check that the value doesn't start with a comma or is empty.
    if ( *p == '\0' || *p == '\r' || *p == '\n' || *p == ',') {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Protocol header");
        return false;
    }
    int i = 0;
    while ( i < 99 && *p != '\0' && *p != ','  && *p != '\r' && *p != '\n') {
        subprotocol[i] = *p;
        i++;
        p++;
    }
    *subprotocol_len = i;
    subprotocol[i] = '\0';
    return true;
}

/**
 * Check the presence and validity of the `Sec-Websocket-Extensions` header in
 * the opening request sent by the client. This header is optional, so absence
 * of the header is valid. Once there's a value, then it has to have a valid
 * format. Send an error response if the checks fails. We return the first
 * extension and its length in the provided parameters.
 * 
 * @param socketfd Client socket descriptor
 * @param buf String buffer containing client request
 * @param extension This will contain the chosen extension
 * @param extension_len This will contain the pointer to the chosen extension
 * length
 * 
 * @return Validity of the header as a boolean
 */
bool get_extensions(int socketfd, char buf[], char extension[],
                                int *extension_len) {
    char *p;
    // Confirm the presence of header
    if ( (p = strcasestr(buf, "Sec-Websocket-Extensions:")) == NULL ) {
        return true;
    }
    // Get the value of the key header
    p += 25; // Increment pointer by the length of `Sec-Websocket-Extensions`
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' || *p == ',') {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Extensions header");
        return false;
    }
    int i = 0;
    while ( i < 99 && *p != '\0' && *p != ','  && *p != '\r' && *p != '\n' ) {
        extension[i] = *p;
        i++;
        p++;
    }
    *extension_len = i;
    extension[i] = '\0';
    return true;
}

void handle_upgrade(int socketfd) {
    int nbytes; // Size Of upgrade request
    char *p;
    char buf[BUFFER_SIZE]; // Buffer that holds request
    char key[60]; // Buffer that holds sec-websocket-accept value
    char subprotocol[100];
    char extension[100];
    int subprotocol_len, extension_len;
    bool is_valid;
    nbytes = recv(socketfd, buf, BUFFER_SIZE, 0);
    subprotocol_len = 0;
    extension_len = 0;

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


    // Confirm the presence of the Host header
    if ( (p = strcasestr(buf, "Host:")) == NULL ) {
        send_error_response(socketfd, 400, "Host header not included");
        return;
    }

    if ( (is_valid = is_upgrade_header_valid(socketfd, buf)) == false ) {
        return;
    }

    if ( (is_valid = get_sec_websocket_key_value(socketfd, buf, key)) == false){
        return;
    }

    if ( (is_valid = is_version_header_valid(socketfd, buf)) == false) {
        return;
    }

    if ( (is_valid = get_subprotocols(socketfd, buf, subprotocol,
                                    &subprotocol_len)) == false) {
        return;
    }

    if ( (is_valid = get_extensions(socketfd, buf, extension,
                                    &extension_len)) == false) {
        return;
    }
    bool sent = __send_upgrade_response(socketfd, key, subprotocol,
                                        subprotocol_len, extension,
                                        extension_len);
    if ( sent == true ) {
        init_client(socketfd);
    }
}

bool __send_upgrade_response(int socketfd, char key[], char subprotocol[],
                            int subprotocol_len, char extension[],
                            int extension_len) {
    int length;
    char response[512];
    unsigned char base64[29];
    char sha1[20];
    // Create `Sec-WebSocket-Accept` value
    strncpy(key+24, GUID, 36);
    SHA1(sha1, key, 60);
    base64_encode((unsigned char *) sha1, base64, 20);
    base64[28] = '\0';

    // Format response based on the presence of subprotocols
    if ( subprotocol_len > 0 ) {
        length = sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\nSec-WebSocket-Protocol: %s\r\n\r\n", base64, subprotocol);
    }
    else {
        length = sprintf(response, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: %s\r\n\r\n", base64);
    }
    response[length] = '\0';
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
        // All data has been read.
        if ( nbytes == total_read ) {
            break;
        }

        if ( client->is_control_frame ) {
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