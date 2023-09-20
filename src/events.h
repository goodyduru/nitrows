#ifndef INCLUDED_EVENTS_DOT_H
#define INCLUDED_EVENTS_DOT_H

#include <stdint.h>
#include <poll.h>

/**
 * We need a portable way to handle events. This allows our server to switch 
 * system specific events handlers or even use a event library in the future.
 * We will enscapulate or events calls here.
 */

typedef struct Event Event;
/**
 * This struct will hold whatever event array that will contain the file 
 * descriptors that we are interested in. It will also hold metadata
 * that will give us the size and count of the array.
*/
struct Event {
    uint64_t count; // Number of file descriptors in the array
    uint64_t size; // Size of the array
    struct pollfd *objects; // Array to hold the file descriptor
};

Event event;
/**
 * This function creates our event loop. We allocate space for 16 of the events 
 * objects.
 */
void init_event_loop();

/**
 * This function adds a file descriptor that we have to watch. It handles
 * increasing the event object array size if there is not enough space.
 * 
 * @param socketfd socket descriptor that is added
*/
void add_to_event_loop(int socketfd);

/**
 * Deletes file descriptor from event loop. We will decrease the event object
 * array size if the number of file descriptors falls below a threshold.
 * 
 * @param socketfd socket descriptor to be deleted
 * @param index index of the socket descriptor. Set to -1 if not known.
*/
void delete_from_event_loop(int socketfd, int index);

/**
 * Runs the event loop. If any of the file descriptor is ready, we handle
 * them with any of the handler functions.
 * 
 * @param listener listener socket
 * @param handle_listener function that runs if it's the listener file
 * descriptor that is ready to be read.
 * @param handle_others function that runs if it's other file desc.
*/
void run_event_loop(int listener, void (*handle_listener)(int), 
                    void (*handle_others(int)));

#endif