#pragma once

#include "event.h"

typedef struct bt_eventproducer BT_EventProducer;
typedef struct bt_eventloop BT_EventLoop;

struct bt_eventproducer {
    int (*on_read)(BT_EventProducer *this, struct bt_event *out);
    int (*on_write)(BT_EventProducer *this, struct bt_event *out);
    int (*on_destroy)(BT_EventProducer *this);
};

#include "util/vector.h"
VECTOR_DEFINE(BT_EventProducer *, evproducer);

struct bt_eventloop {
    int epollfd;
    Vector(evproducer) emitters;
};

int bt_eventloop_init(BT_EventLoop *loop);
int bt_eventloop_run(BT_EventLoop *loop, struct bt_event *out);
int bt_eventloop_register(BT_EventLoop *loop, int fd, BT_EventProducer *producer);
int bt_eventloop_destroy(BT_EventLoop *loop);
