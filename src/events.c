#include <stdlib.h>
#include <stdio.h>

#include "./defs.h"
#include "./events.h"

void init_event_loop() {
    event.count = 0;
    event.size = INITIAL_EVENT_SIZE;
    event.objects = malloc(sizeof(struct pollfd) * INITIAL_EVENT_SIZE);
}

void add_to_event_loop(int socketfd) {
    // If there's no more space, double the size of the event object array.
    if ( event.count == event.size ) {
        event.size *= 2;
        event.objects = realloc(event.objects, sizeof(struct pollfd) * event.size);
    }
    event.objects[event.count].fd = socketfd;
    event.objects[event.count].events = POLLIN;
    event.count++;
}

void delete_from_event_loop(int socketfd, int index) {
    // If index is negative, we need to search for the object containing the
    // socket descriptor.
    if ( index == -1 ) {
        for ( int i = 0; i < event.count; i++ ) {
            if ( event.objects[i].fd == socketfd ) {
                index = i;
                break;
            }
        }
    }

    // Socket wasn't found
    if ( index == -1 )
        return;
    
    // Carry out delete by taking the last item and inserting it into the space
    // occupied by socketfd.
    event.objects[index] = event.objects[event.count-1];
    event.count--;

    // We don't need to reduce size if it is INITIAL_EVENT_SIZE
    if ( event.size == INITIAL_EVENT_SIZE )
        return;
    
    // Reduce size by half if number of items is below a third of the array size
    if ( event.count < event.size/3 ) {
        event.size /= 2;
        event.objects = realloc(event.objects, sizeof(struct pollfd) * event.size);
    }
}

void run_event_loop(int listener, void (*handle_listener)(int),
                    void (*handle_others)(int)) {
    while(1) {
        int poll_count = poll(event.objects, event.count, -1);
        if ( poll_count == -1 ) {
            perror("poll"); // TODO: change this
            exit(1); // Remove this
        }

        for ( int i = 0; i < event.count; i++ ) {
            // Check to see if any is ready and handle them with their
            // respective functions
            if ( event.objects[i].revents & POLLIN ) {
                if ( event.objects[i].fd == listener ) {
                    // We handle listener socket differently.
                    handle_listener(listener);
                } else {
                    handle_others(event.objects[i].fd);
                }
            }
        }
    }
}