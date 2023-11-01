#include "./events.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef __APPLE__
void init_event_loop() {
  kq = kqueue();
  if (kq == -1) {
    perror("kqueue");
    exit(1);
  }
  nitrows_event.count = 0;
  nitrows_event.size = INITIAL_EVENT_SIZE;
  nitrows_event.objects = malloc(sizeof(struct kevent) * INITIAL_EVENT_SIZE);
}

void add_to_event_loop(int socketfd) {
  // If there's no more space, double the size of the event object array.
  if (nitrows_event.count == nitrows_event.size) {
    struct kevent *temp;
    nitrows_event.size *= 2;
    temp = realloc(nitrows_event.objects, sizeof(struct kevent) * nitrows_event.size);
    if (temp != NULL) {
      nitrows_event.objects = temp;
    }
  }
  EV_SET(&nitrows_event.objects[nitrows_event.count++], socketfd, EVFILT_READ, EV_ADD, 0, 0, NULL);
  int err = kevent(kq, nitrows_event.objects, nitrows_event.count, NULL, 0, NULL);
  if (err < 0) {
    printf("Wow!!!\n");
    perror("kevent");
  }
}

void delete_from_event_loop(int socketfd) {
  // Does nothing, for api consistency purpose.
}

void run_event_loop(int listener, void (*handle_listener)(int), void (*handle_others)(int, bool)) {
  struct kevent curr_event;
  while (1) {
    int event_count = kevent(kq, NULL, 0, nitrows_event.outs, INITIAL_EVENT_SIZE, NULL);
    if (event_count == -1) {
      perror("kevent");  // TODO(goody): change this
      exit(1);           // Remove this
    }

    for (int i = 0; i < event_count; i++) {
      curr_event = nitrows_event.outs[i];
      if (curr_event.ident == listener) {
        handle_listener(listener);
      } else {
        if (curr_event.flags & EVFILT_READ) {
          handle_others(curr_event.ident, false);
        } else if (curr_event.flags & EV_EOF) {
          handle_others(curr_event.ident, true);
        }
      }
    }
  }
}
#else
void init_event_loop() {
  nitrows_event.count = 0;
  nitrows_event.size = INITIAL_EVENT_SIZE;
  nitrows_event.objects = malloc(sizeof(struct pollfd) * INITIAL_EVENT_SIZE);
}

void add_to_event_loop(int socketfd) {
  // If there's no more space, double the size of the event object array.
  if (nitrows_event.count == nitrows_event.size) {
    struct pollfd *temp;
    nitrows_event.size *= 2;
    temp = realloc(nitrows_event.objects, sizeof(struct pollfd) * nitrows_event.size);
    if (temp != NULL) {
      nitrows_event.objects = temp;
    }
  }
  nitrows_event.objects[nitrows_event.count].fd = socketfd;
  nitrows_event.objects[nitrows_event.count].events = POLLIN;
  nitrows_event.count++;
}

void delete_from_event_loop(int socketfd) {
  int index = -1;
  // If index is negative, we need to search for the object containing the
  // socket descriptor.
  for (int i = 0; i < nitrows_event.count; i++) {
    if (nitrows_event.objects[i].fd == socketfd) {
      index = i;
      break;
    }
  }

  // Socket wasn't found
  if (index == -1) {
    return;
  }

  // Carry out delete by taking the last item and inserting it into the space
  // occupied by socketfd.
  nitrows_event.objects[index] = nitrows_event.objects[nitrows_event.count - 1];
  nitrows_event.count--;

  // We don't need to reduce size if it is INITIAL_EVENT_SIZE
  if (nitrows_event.size == INITIAL_EVENT_SIZE) {
    return;
  }

  // Reduce size by half if number of items is below a third of the array size
  if (nitrows_event.count < nitrows_event.size / 3) {
    struct pollfd *temp;
    nitrows_event.size /= 2;
    temp = realloc(nitrows_event.objects, sizeof(struct pollfd) * nitrows_event.size);
    if (temp != NULL) {
      nitrows_event.objects = temp;
    }
  }
}

void run_event_loop(int listener, void (*handle_listener)(int), void (*handle_others)(int, bool)) {
  while (1) {
    int poll_count = poll(nitrows_event.objects, nitrows_event.count, -1);
    if (poll_count == -1) {
      perror("poll");  // TODO(goody): change this
      exit(1);         // Remove this
    }

    for (int i = 0; i < nitrows_event.count; i++) {
      // Check to see if any is ready and handle them with their
      // respective functions
      if (nitrows_event.objects[i].revents & POLLIN) {
        if (nitrows_event.objects[i].fd == listener) {
          // We handle listener socket differently.
          handle_listener(listener);
        } else {
          handle_others(nitrows_event.objects[i].fd, false);
        }
      }
    }
  }
}
#endif