#include "./events.h"

#include <stdio.h>
#include <stdlib.h>

#ifdef __linux__
void init_event_loop() {
  epollfd = epoll_create1(0);
  if (epollfd == -1) {
    perror("epoll_create1");
    exit(1);
  }
}

void add_to_event_loop(int socketfd) {
  struct epoll_event ev;
  ev.events = EPOLLIN | EPOLLET;
  ev.data.fd = socketfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, socketfd, &ev) == -1) {
    perror("epoll_ctl: socketfd");
  }
}

void set_write_notify(int socketfd, bool enable) {
  struct epoll_event ev;
  ev.data.fd = socketfd;
  if (enable) {
    ev.events = EPOLLIN | EPOLLET | EPOLLOUT;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, socketfd, &ev) == -1) {
      perror("epoll_ctl: socketfd");
    }
  } else {
    ev.events = EPOLLIN | EPOLLET;
    if (epoll_ctl(epollfd, EPOLL_CTL_MOD, socketfd, &ev) == -1) {
      perror("epoll_ctl: socketfd");
    }
  }
}

void delete_from_event_loop(int socketfd) {
  if (epoll_ctl(epollfd, EPOLL_CTL_DEL, socketfd, NULL) == -1) {
    perror("epoll_ctl: socketfd");
  }
}

void run_event_loop(int listener, void (*handle_listener)(int), void (*handle_others)(int, bool)) {
  struct epoll_event curr_event;
  while (1) {
    int event_count = epoll_wait(epollfd, nitrows_event.objects, INITIAL_EVENT_SIZE, -1);
    if (event_count == -1) {
      perror("epoll_wait");  // TODO(goody): change this
      exit(1);               // Remove this
    }

    for (int i = 0; i < event_count; i++) {
      curr_event = nitrows_event.objects[i];
      if (curr_event.data.fd == listener) {
        handle_listener(listener);
      } else {
        if (curr_event.events & EPOLLIN) {
          handle_others(curr_event.data.fd, false, false);
        } else if (curr_event.events & EPOLLOUT) {
          handle_others(curr_event.data.fd, true, false);
        } else if ((curr_event.events & EPOLLHUP) || (curr_event.events & EPOLLERR)) {
          handle_others(curr_event.data.fd, false, true);
        }
      }
    }
  }
}
#elif defined(__unix__) || defined(__APPLE__)
void init_event_loop() {
  kq = kqueue();
  if (kq == -1) {
    perror("kqueue");
    exit(1);
  }
  nitrows_event.count = 0;
  nitrows_event.size = INITIAL_EVENT_SIZE;
  nitrows_event.objects = malloc(sizeof(struct kevent) * INITIAL_EVENT_SIZE * 2);
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
  EV_SET(&nitrows_event.objects[nitrows_event.count++], socketfd, EVFILT_WRITE, EV_ADD | EV_DISABLE, 0, 0, NULL);
  int err = kevent(kq, nitrows_event.objects, nitrows_event.count, NULL, 0, NULL);
  if (err < 0) {
    perror("kevent");
  }
}

void set_write_notify(int socketfd, bool enable) {
  int i;
  for (i = 0; i < nitrows_event.count; i++) {
    if (nitrows_event.objects[i].ident == socketfd) {
      break;
    }
  }

  if (i == nitrows_event.count) {
    return;
  }

  if (enable) {
    nitrows_event.objects[i + 1].flags &= ~EV_DISABLE;
    nitrows_event.objects[i + 1].flags |= EV_ENABLE;
  } else {
    nitrows_event.objects[i + 1].flags &= ~EV_ENABLE;
    nitrows_event.objects[i + 1].flags |= EV_DISABLE;
  }

  int err = kevent(kq, nitrows_event.objects, nitrows_event.count, NULL, 0, NULL);
  if (err < 0) {
    perror("kevent");
  }
}

void delete_from_event_loop(int socketfd) {
  int index = -1;
  // If index is negative, we need to search for the object containing the
  // socket descriptor.
  for (int i = 0; i < nitrows_event.count; i++) {
    if (nitrows_event.objects[i].ident == socketfd) {
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
  nitrows_event.objects[index] = nitrows_event.objects[nitrows_event.count - 2];
  nitrows_event.objects[index + 1] = nitrows_event.objects[nitrows_event.count - 1];
  nitrows_event.count -= 2;

  // We don't need to reduce size if it is INITIAL_EVENT_SIZE
  if (nitrows_event.size == INITIAL_EVENT_SIZE) {
    return;
  }

  // Reduce size by half if number of items is below a third of the array size
  if (nitrows_event.count < nitrows_event.size / 3) {
    struct kevent *temp;
    nitrows_event.size /= 2;
    temp = realloc(nitrows_event.objects, sizeof(struct kevent) * nitrows_event.size);
    if (temp != NULL) {
      nitrows_event.objects = temp;
    }
  }
}

void run_event_loop(int listener, void (*handle_listener)(int), void (*handle_others)(int, bool, bool)) {
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
        if (curr_event.filter == EVFILT_READ) {
          handle_others(curr_event.ident, false, false);
        } else if (curr_event.filter == EVFILT_WRITE) {
          handle_others(curr_event.ident, true, false);
        } else if (curr_event.flags & EV_EOF) {
          handle_others(curr_event.ident, false, true);
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

void set_write_notify(int socketfd, bool enable) {
  int i;
  for (i = 0; i < nitrows_event.count; i++) {
    if (nitrows_event.objects[i].fd == socketfd) {
      break;
    }
  }

  if (i == nitrows_event.count) {
    return;
  }

  if (enable) {
    nitrows_event.objects[i].events |= POLLOUT;
  } else {
    nitrows_event.objects[i].events &= ~POLLOUT;
  }
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

void run_event_loop(int listener, void (*handle_listener)(int), void (*handle_others)(int, bool, bool)) {
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
          handle_others(nitrows_event.objects[i].fd, false, false);
        }
      } else if (nitrows_event.objects[i].revents & POLLOUT) {
        handle_others(nitrows_event.objects[i].fd, true, false);
      }
    }
  }
}
#endif