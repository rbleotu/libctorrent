#include <fcntl.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <poll.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "eventloop.h"

#define MAX_EVENTS 16

int
bt_eventloop_init(BT_EventLoop *loop)
{
    assert (loop != NULL);
    loop->epollfd = -1;
    loop->emitters = (Vector(evproducer)) VECTOR();

    loop->epollfd = epoll_create1(0);
    if (loop->epollfd == -1) {
       perror("epoll_create1");
       return -1;
    }

    return 0;
}

int
bt_eventloop_register(BT_EventLoop *loop, int fd, BT_EventProducer *prod)
{
    assert (loop != NULL);
    assert (fd != -1);

    const ssize_t n = vector_pushback(&loop->emitters, prod);
    if (n < 0) {
        bt_errno = BT_EALLOC;
        return -1;
    }

    struct epoll_event ev = {
        .events = (prod->on_read ? EPOLLIN : 0) | (prod->on_write ? EPOLLOUT : 0),
        .data.ptr = prod,
    };

    if (epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int
bt_eventloop_run(BT_EventLoop *loop)
{
    int n;
    struct epoll_event events[MAX_EVENTS];

    for (;;) {
        const int nfds = epoll_wait(loop->epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            return -1;
        }

        for (n = 0; n < nfds; ++n) {
            BT_EventProducer *prod = events[n].data.ptr;
            assert (prod != NULL);

            if (events[n].events & EPOLLIN) {
                const int event = prod->on_read(prod);
                if (event)
                    return event;
            }

            if (events[n].events & EPOLLOUT) {
                const int event = prod->on_write(prod);
                if (event)
                    return event;
            }

        }
    }

    assert (!"unreachable code");
    return 0;
}

int bt_eventloop_destroy(BT_EventLoop *loop)
{
    assert (!"not implemented");
    return 0;
}
