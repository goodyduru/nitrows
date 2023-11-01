#include <netdb.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>

#include "defs.h"
#include "events.h"
#include "net.h"

int get_listener_socket() {
    int listener = 0; // Listener socket descriptor
    int yes = 1; // We need it to setup SO_REUSEADDR 
    int rv = 0;

    struct addrinfo hints;
    struct addrinfo *ai;
    struct addrinfo *p;

    // Get a socket from the OS and bind it
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    rv = getaddrinfo(NULL, PORT, &hints, &ai);
    if ( rv != 0 ) {
        printf("Error %s", gai_strerror(rv));
        return -1;
    }

    // Create a socket from one of the addrinfo in the `ai` linked list and
    // bind to it.
    for ( p = ai; p != NULL; p = p->ai_next ) {
        listener = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if ( listener < 0 ) {
            continue;
        }

        // Set up SO_REUSEADDR to avoid "address already in use" error message
        setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if ( bind(listener, p->ai_addr, p->ai_addrlen) < 0 ) {
            close(listener);
            continue;
        }

        break;
    }

    freeaddrinfo(ai);

    // Return error if we didn't get bound
    if ( p == NULL ) {
        perror("bind");
        return -1;
    }

    if ( listen(listener, LISTEN_BACKLOG) == -1 ) {
        perror("listen");
        return -1;
    }

    return listener;
}

// Get either IPv4 or IPv6 sockaddr.
void *get_in_addr(struct sockaddr *sa) {
    // Get IPv4 sockaddr
    if ( sa->sa_family == AF_INET ) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    // Get IPv6 sockaddr
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void accept_connection(int listener_socket) {
    int newfd = 0;
    struct sockaddr_storage remote_addr;
    socklen_t addrlen;

    char ip_addr[INET6_ADDRSTRLEN];

    addrlen = sizeof remote_addr;
    newfd = accept(listener_socket, (struct sockaddr *)&remote_addr, &addrlen);
    if ( newfd ==  -1 ) {
        printf("accept");
    }
    add_to_event_loop(newfd);
    printf("New connection from %s\n", inet_ntop(remote_addr.ss_family, get_in_addr((struct sockaddr*)&remote_addr), ip_addr, INET6_ADDRSTRLEN));
}
