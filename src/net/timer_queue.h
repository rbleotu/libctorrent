#ifndef TIMER_QUEUE_H_
#define TIMER_QUEUE_H_

#include <signal.h>
typedef struct bt_tqueue *BT_TQueue;

struct bt_timer {
    unsigned nsec;
    void *data;
    void (*handler)(void *data);
};

#define BT_TIMER(sec, d, act) \
    ((struct bt_timer){(sec), (d), (void (*)(void *))(act)})

BT_TQueue
bt_tqueue_new(size_t cap);

int
bt_tqueue_insert(BT_TQueue tq, struct bt_timer tm);

struct bt_timer
bt_tqueue_remove(BT_TQueue tq);

void *
bt_timerthread(BT_TQueue tq);

#endif
