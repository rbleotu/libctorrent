#pragma once

#include <time.h>
#include "eventloop.h"
#include "event.h"

#define TIMERQUEUE_CAP 128
#define TIMER(v,e) ((struct bt_timer){\
        {.it_value = {                \
            .tv_sec = (v),            \
            .tv_nsec = 0,             \
        }}, (e)})

typedef struct bt_timerqueue BT_TimerQueue;
typedef struct bt_timer *BT_Timer;

struct bt_timer {
    struct itimerspec value;
    BT_Event payload;
};

struct bt_timerqueue {
    struct bt_eventproducer emitter;
    BT_EventLoop *eloop;

    int timerfd;

    BT_Timer queue[TIMERQUEUE_CAP];
    size_t ntimers;
};

int bt_timerqueue_init(BT_TimerQueue *tq, BT_EventLoop *eloop);
BT_Timer bt_timerqueue_insert(BT_TimerQueue *tq, BT_Timer timer);
int bt_timerqueue_extend(BT_TimerQueue *tq, BT_Timer timer, int value);
int bt_timerqueue_remove(BT_TimerQueue *tq, BT_Timer timer);

