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

int16_t parse_extensions(int socketfd, char *start,
                        ExtensionList *extension_list) {
    bool IN_QUOTE = false;
    bool IN_ESCAPE = false;
    char *p = start;
    char error[] = "Invalid Sec-Websocket-Extensions header";

    while ( *p == ' ' || *p == '\t' ) {
        p++;
    }
    if ( *p == '\0' || *p == '\r' || *p == '\n' || *p == ',') {
        send_error_response(socketfd, 400, error);
        return -1;
    }
    char temp[EXTENSION_VALUE_LENGTH];
    char key[EXTENSION_KEY_LENGTH];
    uint16_t i = 0;
    uint16_t j = 0;
    bool hasExtension = false;
    char c, *params;
    while ( *p != '\0' && *p != '\r' && *p != '\n' ) {
        c = *p;
        if ( !hasExtension && i == (EXTENSION_KEY_LENGTH - 1) ) {
            send_error_response(socketfd, 400, error);
            return -1;
        }
        if ( hasExtension && j == (EXTENSION_VALUE_LENGTH - 1) ) {
            send_error_response(socketfd, 400, error);
            return -1;
        }
        if ( c == '"' && !IN_ESCAPE ) {
            IN_QUOTE = !IN_QUOTE;
            if ( hasExtension ) {
                temp[j] = c;
                j++;
            } else {
                temp[i] = c;
                i++;
            }
        } else if ( c == '\\' && !IN_QUOTE ) {
            IN_ESCAPE = !IN_ESCAPE;
            if ( hasExtension ) {
                temp[j] = c;
                j++;
            } else {
                temp[i] = c;
                i++;
            }
        } else if ( !hasExtension && ( c == ' ' || c == '\t') && i == 0 ) {
            p++;
            continue;
        } else if ( !hasExtension && c == ';' && !IN_QUOTE ) {
            hasExtension = true;
            key[i] = '\0';
        } else if ( !hasExtension && c == ',' && !IN_QUOTE ) {
            // Extension is empty. We add to token list
            key[i] = '\0';
            params = get_extension_params(extension_list, key, true);
            size_t param_len = strlen(params);
            if ( (param_len + j + 1) >= EXTENSION_VALUE_LENGTH ) {
                send_error_response(socketfd, 400, error);
                return -1;
            }
            params[param_len] = ',';
            param_len++;
            params[param_len] = '\0';
            i = 0;
        } else if ( !hasExtension ) {
            key[i] = c;
            i++;
        } else if ( hasExtension && c == ',' && !IN_QUOTE ) {
            temp[j] = '\0';
            params = get_extension_params(extension_list, key, true);
            size_t param_len = strlen(params);
            if ( (param_len + j + 1) >= EXTENSION_VALUE_LENGTH ) {
                send_error_response(socketfd, 400, error);
                return -1;
            }
            if ( param_len > 0 ) {
                params[param_len] = ',';
                param_len++;
            }  
            strcpy(params+param_len, temp);
            hasExtension = false;
            i = 0;
            j = 0;
        }  else if ( hasExtension && ( c == ' ' || c == '\t') && j == 0 ) {
            p++;
            continue;
        } else if ( hasExtension ) {
            temp[j] = c;
            j++;
        }
        p++;
        continue;
    }

    if ( !hasExtension && i > 0 ) {
        key[i] = '\0';
        params = get_extension_params(extension_list, key, true);
        size_t param_len = strlen(params);
        if ( (param_len + j + 1) >= EXTENSION_VALUE_LENGTH ) {
            send_error_response(socketfd, 400, error);
            return -1;
        }
        params[param_len] = ',';
        param_len++;
        params[param_len] = '\0';
    } else if ( hasExtension && j > 0 ) {
        key[j] = '\0';
        params = get_extension_params(extension_list, key, true);
        size_t param_len = strlen(params);
        if ( (param_len + j + 1) >= EXTENSION_VALUE_LENGTH ) {
            send_error_response(socketfd, 400, error);
            return -1;
        }
        if ( param_len > 0 ) {
            params[param_len] = ',';
            param_len++;
        }  
        strcpy(params+param_len, temp);
    }
    return p - start;
}

bool validate_headers(char buf[], int socketfd, char key[], char subprotocol[],
                      int subprotocol_len, char extension[],
                      int extension_len) {
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
    return true;
}
