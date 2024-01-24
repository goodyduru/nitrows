# Nitrows: A WebSocket Server
Nitrows is very easy to use. Here's how

    ```C
        #include "nitrows.h"
        
        void echo_message(int client_id, uint8_t *message, uint64_t length) {
            bool is_sent = nitrows_send_message(client_id, message, length);
            if (!is_sent) {
                nitrows_close(client_id);
            }
        }

        int main() {
            nitrows_set_message_handler(echo_message);
            nitrows_run();
        }
    ```

You create a message handler function that accepts 3 parameters, the client_id, the Websocket message, and the size of the message. You pass that function as a parameter to the `nitrows_set_message_handler` function, then run the `nitrows_run` function. And that's it, you now have a working websocket server.

Ensure you have openssl and zlib on your system.

## Introduction
Nitrows is a websocket server written in C. In its current stage, **I wouldn't recommend its use in production environment**. The purpose of this project was to further my understanding of the Websocket protocol. In addition, I wanted to implement all I learnt from the [MIT's Performance Engineering](https://ocw.mit.edu/courses/6-172-performance-engineering-of-software-systems-fall-2018/) course. I can say I fulfilled all of them. It's why nitro is in its name 😁.

## Features
Nitrows provides a complete server-side implementation of the Websocket specification. It supports permessage-default. It doesn't provide TLS, you'd have to use a reverse proxy server for that.

## Testing
Nitrows passes all the server related tests in the [Autobahn Testsuite](https://github.com/crossbario/autobahn-testsuite). That's all the tests it needs.
