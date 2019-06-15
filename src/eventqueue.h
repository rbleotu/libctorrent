#pragma once

#include "event.h"

#define EVENTQUEUE_MAX 128
#define EVENTQUEUE_INIT() ((struct bt_eventqueue) {\
            .qhead = 0,                            \
            .qtail = 0,                            \
            .n = 0,                                \
        })

typedef struct bt_eventqueue BT_EventQueue;

struct bt_eventqueue {
    struct bt_event queue[128];
    int qhead;
    int qtail;
    int n;
};

static inline int
bt_eventqueue_init(BT_EventQueue *queue)
{
    *queue = EVENTQUEUE_INIT();
}

static inline int
bt_eventqueue_push(BT_EventQueue *queue, struct bt_event e)
{
    assert (queue->n < EVENTQUEUE_MAX);
    if (queue->qtail >= EVENTQUEUE_MAX)
        queue->qtail = 0;
    queue->queue[queue->qtail++] = e;
    return ++queue->n;
}

static inline bool bt_eventqueue_isempty(BT_EventQueue *queue)
{
    return queue->n == 0;
}

static inline int
bt_eventqueue_pop(BT_EventQueue *queue, struct bt_event *e)
{
    if (!queue->n)
        return BT_EVNONE;
    if (queue->qhead >= EVENTQUEUE_MAX)
        queue->qhead = 0;
    const int type = queue->queue[queue->qhead].type;
    if (e)
        *e = queue->queue[queue->qhead];
    ++queue->qhead;
    --queue->n;
    return type;
}
