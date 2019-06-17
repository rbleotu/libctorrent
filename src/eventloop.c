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
#include "event.h"
#include "eventloop.h"

#define MAX_EVENTS 256

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

    int flags = 0;
    if (prod->on_read)
        flags |= EPOLLIN;
    if (prod->on_write)
        flags |= EPOLLOUT;
    assert (flags != 0);

    struct epoll_event ev = {.events = flags, .data.ptr = prod,};

    if (epoll_ctl(loop->epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        perror("epoll_ctl:");
        exit(EXIT_FAILURE);
    }

    return 0;
}

int
bt_eventloop_run(BT_EventQueue *queue, BT_EventLoop *loop)
{
    struct epoll_event events[MAX_EVENTS];
    int err = 0;

    while (bt_eventqueue_isempty(queue)) {
        const int n = epoll_wait(loop->epollfd, events, MAX_EVENTS, -1);
        if (n == -1) {
            perror("epoll_wait");
            return -1;
        }

        for (int i = 0; i < n; ++i) {
            BT_EventProducer *prod = events[i].data.ptr;
            assert (prod != NULL);

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                if (prod->on_destroy) {
                    err = prod->on_destroy(prod, queue);
                    if (err < 0) {
                        perror("error ocurred");
                    }
                }
            }

            if (events[i].events & EPOLLIN) {
                if ((err = prod->on_read(prod, queue)) < 0) {
                    perror("read()");
                }
            }

            if (events[i].events & EPOLLOUT) {
                if ((err = prod->on_write(prod, queue)) < 0) {
                    perror("write()");
                }
            }
        }
    }

    return err;
}

int bt_eventloop_destroy(BT_EventLoop *loop)
{
    assert (!"not implemented");
    return 0;
}

bool bt_eventloop_unregister(BT_EventLoop *loop, int fd, BT_EventProducer *producer)
{
    size_t i = 0;

    VECTOR_FOREACH (&loop->emitters, e, BT_EventProducer *) {
        if (*e == producer) {
            goto remove;
        }
        i  = i + 1;
    }

    return false;
remove:
    epoll_ctl(loop->epollfd, EPOLL_CTL_DEL, fd, NULL);
    vector_remove(&loop->emitters, i);
    return true;
}

int bt_eventloop_oneshot(BT_EventLoop *loop, int fd, BT_EventProducer *prod)
{
    assert (loop != NULL);
    assert (fd != -1);

    int flags = EPOLLONESHOT;
    if (prod->on_read)
        flags |= EPOLLIN;
    if (prod->on_write)
        flags |= EPOLLOUT;

    struct epoll_event ev = {.events = flags, .data.ptr = prod,};
    if (epoll_ctl(loop->epollfd, EPOLL_CTL_MOD, fd, &ev) == -1) {
        perror("epoll_ctl:");
        exit(EXIT_FAILURE);
    }

    return 0;
}
