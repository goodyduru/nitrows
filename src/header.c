#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "header.h"
#include "server.h"

int16_t move_to_next_line(char *start) {
    char *p = start;
    // Look for carriage return
    while ( *p != '\r' ) p++;
    // Invalid header if it isn't followed by '\n'
    if ( *p != '\r' && *(p+1) != '\n' ) {
        return -1;
    }

    p += 2; // Move to the next line
    return p - start;
}

int16_t is_upgrade_header_valid(int socketfd, char *start) {
    int cmp;
    char *p = start;

    // Search for the presence of the first non-whitespace character
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' ) {
        send_error_response(socketfd, 400, "Invalid Upgrade header");
        return -1;
    }
    if ( (cmp = strncasecmp(p, "websocket", 9)) != 0 ) {
        send_error_response(socketfd, 400, "Invalid Upgrade header value");
        return -1;
    }
    p += 9;
    return p - start;
}

int16_t get_sec_websocket_key_value(int socketfd, char *start, char key[]) {
    char c;
    char *p = start;
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' ) {
        send_error_response(socketfd, 400, "Invalid Sec-Websocket-Key header");
        return -1;
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
        return -1;
    }
    // Valid Sec-Websocket-Key value must end with 2 '=' characters
    if ( *p != '=' || *(p+1) != '=' ) {
        send_error_response(socketfd, 400, 
                            "Invalid Sec-Websocket-Key header value");
        return -1;
    }
    key[22] = '=';
    key[23] = '=';
    p += 2;
    return p - start;
}

int16_t is_version_header_valid(int socketfd, char *start) {
    int cmp;
    char *p = start;
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' ) {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Version header");
        return -1;
    }
    if ( (cmp = strncasecmp(p, "13", 2)) != 0 ) {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Version header value");
        return -1;
    }
    p += 2;
    return p - start;
}

int16_t get_subprotocols(int socketfd, char *start, char subprotocol[],
                                int *subprotocol_len) {
    char *p = start;
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }

    // Check that the value doesn't start with a comma or is empty.
    if ( *p == '\0' || *p == '\r' || *p == '\n' || *p == ',') {
        send_error_response(socketfd, 400,
                            "Invalid Sec-Websocket-Protocol header");
        return -1;
    }
    int i = 0;
    while ( i < 99 && *p != '\0' && *p != ','  && *p != '\r' && *p != '\n') {
        subprotocol[i] = *p;
        i++;
        p++;
    }
    *subprotocol_len = i;
    subprotocol[i] = '\0';
    return p - start;
}

int16_t parse_extensions(int socketfd, char *line,
                        ExtensionList *extension_list) {
    bool IN_QUOTE = false;
    char *p = line;
    char error[] = "Invalid Sec-Websocket-Extensions header";
    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' || *p == ',' || *p == ';') {
        send_error_response(socketfd, 400, error);
        return -1;
    }
    char *start = NULL;
    int8_t i, key_length, length = 0;
    bool is_digit, has_extension = false;
    char c, key[EXTENSION_TOKEN_LENGTH+1];
    ExtensionParam *prev_params, *current_params = NULL;
    while ( *p != '\0' && *p != '\r' && *p != '\n' ) {
        c = *p;
        if ( c == '"' ) {
            IN_QUOTE = !IN_QUOTE;
        } else if ( !has_extension && ( c == ' ' || c == '\t') && start == NULL && !IN_QUOTE ) {
            p++;
            continue;
        } else if ( !has_extension && length == 0 && (c == ',' || c == ';') && !IN_QUOTE ) {
            send_error_response(socketfd, 400, error);
            return -1;
        } else if ( !has_extension && ( c == ';' || c == ',') && !IN_QUOTE && start != NULL ) {
            if ( length > EXTENSION_TOKEN_LENGTH ) {
                send_error_response(socketfd, 400, error);
                return -1;
            }
            strncpy(key, start, length);
            key[length] = '\0';
            start = NULL;
            length = 0;
            current_params = get_extension_params(extension_list, key, true);
            while ( current_params != NULL && current_params->value_type != EMPTY ) {
                prev_params = current_params;
                current_params = current_params->next;
            }
            if ( current_params == NULL ) {
                prev_params->next = calloc(1, sizeof(ExtensionParam));
                current_params = prev_params->next;
            }
            if ( c == ',' ) {
                current_params->value_type = BOOL;
                current_params->bool_type = true;
                current_params->is_last = true;
            } else {
                has_extension = true;
            }
        } else if ( has_extension && (c == ',' || c == ';' || c == '=') && !IN_QUOTE && start != NULL ) {
            if ( length > EXTENSION_TOKEN_LENGTH ) {
                send_error_response(socketfd, 400, error);
                return -1;
            }
            if ( current_params->value_type != EMPTY ) {
                current_params->next = calloc(1, sizeof(ExtensionParam));
                current_params = current_params->next;
            }
            key_length = strlen(current_params->key);
            is_digit = true;
            for (i = 0; i < length; i++) {
                is_digit &= isdigit(*(start+i));
                if ( !is_digit ) {
                    break;
                }
            }
            if ( key_length == 0 && !is_digit ) {
                strncpy(current_params->key, start, length);
                current_params->key[length] = '\0';
                if (c == ',' || c == ';') {
                    current_params->value_type = BOOL;
                    current_params->bool_type = true;
                }
            } else if ( is_digit ) {
                if ( c == '=' ) {
                    strncpy(current_params->key, start, length);
                    current_params->string_type[length] = '\0';
                    current_params->value_type = STRING;
                }
                else {
                    for ( i = 0; i < length; i++ ) {
                        current_params->int_type = current_params->int_type*10 + (*(start+i) - '0');
                    }
                    current_params->value_type = INT;
                }
            } else if ( key_length > 0 ) {
                strncpy(current_params->string_type, start, length);
                current_params->string_type[length] = '\0';
                current_params->value_type = STRING;
            }
            if ( c == ',' ) {
                current_params->is_last = true;
                has_extension = false;
            }
            start = NULL;
            length = 0;
        }  else if ( has_extension && ( c == ' ' || c == '\t') && start == NULL && !IN_QUOTE ) {
            p++;
            continue;
        } else {
            if ( start == NULL ) {
                start = p;
            }
            length++;
        }
        p++;
        continue;
    }
    if ( start == NULL || IN_QUOTE ) {
         return p - line;
    }
    if ( length > EXTENSION_TOKEN_LENGTH ) {
        send_error_response(socketfd, 400, error);
        return -1;
    }
    if ( !has_extension ) {
        strncpy(key, start, length);
        key[length] = '\0';
        current_params = get_extension_params(extension_list, key, true);
        while ( current_params != NULL && current_params->value_type != EMPTY ) {
            prev_params = current_params;
            current_params = current_params->next;
        }
        if ( current_params == NULL ) {
            prev_params->next = calloc(1, sizeof(ExtensionParam));
            current_params = prev_params->next;
        }
        current_params->value_type = BOOL;
        current_params->bool_type = true;
        current_params->is_last = true;
    } 
    else {
        if ( current_params->value_type != EMPTY ) {
            current_params->next = calloc(1, sizeof(ExtensionParam));
            current_params = current_params->next;
        }
        key_length = strlen(current_params->key);
        is_digit = true;
        for (i = 0; i < length; i++) {
            is_digit &= isdigit(*(start+i));
            if ( !is_digit ) {
                break;
            }
        }
        if ( key_length == 0 && !is_digit ) {
            strncpy(current_params->key, start, length);
            current_params->key[length] = '\0';
            current_params->value_type = BOOL;
            current_params->bool_type = true;
        } else if ( is_digit ) {
            for ( i = 0; i < length; i++ ) {
                current_params->int_type = current_params->int_type*10 + (*(start+i) - '0');
            }
            current_params->value_type = INT;
        } else if ( key_length > 0 ) {
            strncpy(current_params->string_type, start, length);
            current_params->string_type[length] = '\0';
            current_params->value_type = STRING;
        }
        current_params->is_last = true;
    }
    return p - line;
}

bool validate_headers(char buf[], int socketfd, char key[], char subprotocol[],
                      int subprotocol_len, uint8_t **extension_indices,
                      uint8_t *indices_count) {
    char *p;
    int8_t index;
    int16_t progress;
    size_t request_length;
    char *headers[] = {"Host:", "Upgrade:", "Sec-Websocket-Key:",           
                       "Sec-Websocket-Version:", "Sec-Websocket-Protocol:", 
                       "Sec-Websocket-Extensions:"};
    int8_t header_count = 6;
    bool required_headers_present[] = {false, false, false, false}; // Store required headers check status here.
    p = buf;
    request_length = strlen(buf);
    ExtensionList *list = get_extension_list(socketfd);

    while ( *p != '\0' && (p - buf) < request_length && 
            *p != '\r' && *p != '\n' ) {
        for ( index = 0; index < header_count && (p - buf) < request_length; index++ ) {
            if ( index < 4 && required_headers_present[index] ) {
                continue;
            }
            if ( strncasecmp(p, headers[index], strlen(headers[index])) != 0 ) {
                continue;
            }
            p += strlen(headers[index]); // Bypass header
            if ( index == 0 ) {
                // Host is available. That's all we want to know
                required_headers_present[index] = true;
                break;
            } else if ( index == 1 ) {
                if ( (progress = is_upgrade_header_valid(socketfd, p)) == -1 ) {
                    return false;
                }
                p += progress;
                required_headers_present[index] = true;
                break;
            } else if ( index == 2 ) {
                if ( (progress = get_sec_websocket_key_value(socketfd, p, key)) == -1){
                    return false;
                }
                p += progress;
                required_headers_present[index] = true;
                break;
            } else if ( index == 3 ) {
                if ( (progress = is_version_header_valid(socketfd, p)) == -1) {
                    return false;
                }
                p += progress;
                required_headers_present[index] = true;
                break;
            } else if ( index == 4 ) {
                if ( (progress = get_subprotocols(socketfd, p, subprotocol,
                                        &subprotocol_len)) == -1) {
                    return false;
                }
                p += progress;
                break;
            } else {
                if ( (progress = parse_extensions(socketfd, p, list)) == -1) {
                    return false;
                }
                p += progress;
                break;
            }
        }
        if ( (progress = move_to_next_line(p)) == -1 ) {
            return false;
        }
        p += progress;
    }

    // If none of the required headers isn't there, return false
    index = 0;
    while (index < 4) {
        if ( !required_headers_present[index] ) {
            return false;
        }
        index++;
    }

    bool is_valid = validate_extension_list(socketfd, list, extension_indices,
                                            indices_count);
    delete_extension_list(socketfd);
    return is_valid;
}
