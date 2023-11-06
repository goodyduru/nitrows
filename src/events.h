#ifndef NITROWS_SRC_EVENTS_H
#define NITROWS_SRC_EVENTS_H

#include <stdbool.h>
#include <stdint.h>
#ifdef __linux__
#include <sys/epoll.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/event.h>
#include <sys/types.h>
#else
#include <poll.h>
#endif

// Initial number of sockets to be monitored by our event library.
#define INITIAL_EVENT_SIZE 16

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
#ifdef __linux__
struct Event {
  struct epoll_event objects[INITIAL_EVENT_SIZE];
} static int epollfd;
#elif defined(__unix__) || defined(__APPLE__)
struct Event {
  uint64_t count;
  uint64_t size;
  struct kevent *objects;
  struct kevent outs[INITIAL_EVENT_SIZE];
};
static int kq;
#else
struct Event {
  uint64_t count;          // Number of file descriptors in the array
  uint64_t size;           // Size of the array
  struct pollfd *objects;  // Array to hold the file descriptor
};
#endif

static Event nitrows_event;
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
 * This function enables or disables the write availability detection for a socket.
 *
 * @param socketfd socket descriptor to enable or disable write availability detection
 * @param enable Boolean to enable or disable write availability detection.
 */
void set_write_notify(int socketfd, bool enable);

/**
 * Deletes file descriptor from event loop. We will decrease the event object
 * array size if the number of file descriptors falls below a chosen threshold.
 *
 * @param socketfd socket descriptor to be deleted
 */
void delete_from_event_loop(int socketfd);

/**
 * Runs the event loop. If any of the file descriptor is ready, we handle
 * them with any of the handler functions.
 *
 * @param listener listener socket
 * @param handle_listener function that runs if it's the listener socket
 * that is ready to be read.
 * @param handle_others function that runs if it's other sockets.
 */
void run_event_loop(int listener, void (*handle_listener)(int), void (*handle_others)(int, bool, bool));
#endif