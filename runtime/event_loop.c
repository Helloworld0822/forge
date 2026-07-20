#include "forge/event.h"
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <unistd.h>

struct fr_event_loop {
    int epfd;
    fr_event_cb_t cb;
};

fr_event_loop_t *fr_event_loop_create(void) {
    fr_event_loop_t *loop = (fr_event_loop_t *)calloc(1, sizeof(fr_event_loop_t));
    if (!loop) return NULL;
    loop->epfd = epoll_create1(0);
    if (loop->epfd < 0) {
        free(loop);
        return NULL;
    }
    return loop;
}

void fr_event_loop_destroy(fr_event_loop_t *loop) {
    if (!loop) return;
    if (loop->epfd >= 0) close(loop->epfd);
    free(loop);
}

void fr_event_loop_set_cb(fr_event_loop_t *loop, fr_event_cb_t cb) {
    if (loop) loop->cb = cb;
}

int fr_event_loop_add(fr_event_loop_t *loop, int fd, uint32_t events, void *userdata) {
    if (!loop || fd < 0) return -1;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = userdata;
    return epoll_ctl(loop->epfd, EPOLL_CTL_ADD, fd, &ev);
}

int fr_event_loop_mod(fr_event_loop_t *loop, int fd, uint32_t events, void *userdata) {
    if (!loop || fd < 0) return -1;
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.ptr = userdata;
    return epoll_ctl(loop->epfd, EPOLL_CTL_MOD, fd, &ev);
}

int fr_event_loop_del(fr_event_loop_t *loop, int fd) {
    if (!loop || fd < 0) return -1;
    return epoll_ctl(loop->epfd, EPOLL_CTL_DEL, fd, NULL);
}

int fr_event_loop_poll(fr_event_loop_t *loop, int timeout_ms) {
    if (!loop) return -1;
    struct epoll_event events[64];
    int n = epoll_wait(loop->epfd, events, 64, timeout_ms);
    if (n < 0) return -1;
    for (int i = 0; i < n; i++) {
        if (loop->cb) {
            loop->cb(loop, 0, events[i].events, events[i].data.ptr);
        }
    }
    return n;
}
