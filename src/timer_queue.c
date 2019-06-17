#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <sys/timerfd.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "util/common.h"
#include "util/error.h"
#include "timer_queue.h"

#define swap(a,b) do{                 \
    typeof(a) _tmp;                   \
    _tmp = (b), (b) = (a), (a) = _tmp;\
} while(0)

local int
timerqueue_getrem(BT_TimerQueue *tq, struct itimerspec *out)
{
    const int ec = timerfd_gettime(tq->timerfd, out);
    if (ec) {
        perror("timerfd_gettime");
        return -1;
    }
    return 0;
}

local int
timerqueue_arm(BT_TimerQueue *tq)
{
    if (!tq->ntimers)
        return 0;

    BT_Timer front = tq->queue[0];

    struct itimerspec ispec = front->value;

    if (timerfd_settime(tq->timerfd, 0, &ispec, NULL) < 0) {
        perror("timerfd_settime()");
        return -1;
    }

    bt_eventloop_register(tq->eloop, tq->timerfd, &tq->emitter);
    return 0;
}

local int
timerqueue_disarm(BT_TimerQueue *tq)
{
    if  (!tq->ntimers)
        return 0;

    const int ec = bt_eventloop_unregister(tq->eloop, tq->timerfd, &tq->emitter);
    if (ec < 0) {
        perror("unregister()");
        return -1;
    }

    struct itimerspec rem;
    if (timerqueue_getrem(tq, &rem) < 0) {
        return -1;
    }

    tq->queue[0]->value = rem;
}

local BT_Timer
pop(BT_Timer v[], size_t n)
{
    BT_Timer front = v[0];
    memmove(v, v+1, sizeof(BT_Timer [n-1]));
    return front;
}

local int
timer_fire(BT_EventProducer *prod, BT_EventQueue *queue)
{
    BT_TimerQueue *this = container_of(prod, struct bt_timerqueue, emitter);

    timerqueue_disarm(this);
    BT_Timer front = pop(this->queue, this->ntimers--);
    bt_eventqueue_push(queue, front->payload);
    timerqueue_arm(this);

    return true;
}

#define NSEC (1000000000ULL)

local int
timer_cmp(BT_Timer a, BT_Timer b)
{
    uint64 totala = a->value.it_value.tv_sec * NSEC + a->value.it_value.tv_nsec;
    uint64 totalb = b->value.it_value.tv_sec * NSEC + b->value.it_value.tv_nsec;
    return totala > totalb ? 1 : totala < totalb ? -1 : 0;
}

local void
timer_sub(BT_Timer a, BT_Timer b)
{
    uint64 totala = a->value.it_value.tv_sec * NSEC + a->value.it_value.tv_nsec;
    uint64 totalb = b->value.it_value.tv_sec * NSEC + b->value.it_value.tv_nsec;
    uint64 diff = totala - totalb;
    a->value.it_value = (struct timespec) {
        .tv_sec = diff / NSEC,
        .tv_nsec = diff % NSEC,
    };
}

local void
timer_add(BT_Timer a, BT_Timer b)
{
    uint64 totala = a->value.it_value.tv_sec * NSEC + a->value.it_value.tv_nsec;
    uint64 totalb = b->value.it_value.tv_sec * NSEC + b->value.it_value.tv_nsec;
    uint64 sum = totala + totalb;
    a->value.it_value = (struct timespec) {
        .tv_sec = sum / NSEC,
        .tv_nsec = sum % NSEC,
    };
}

local void
sink(BT_Timer timer, BT_Timer v[], size_t n)
{
    size_t i = 0;

    while (i < n) {
        if (timer_cmp(timer, v[i]) < 0) {
            timer_sub(v[i], timer);
            break;
        } else {
            timer_sub(timer, v[i]);
        }
        i = i + 1;
    }

    memmove(v+i+1, v+i, sizeof(BT_Timer [n-i]));
    v[i] = timer;
}

BT_Timer
bt_timerqueue_insert(BT_TimerQueue *tq, BT_Timer timer)
{
    assert (tq->ntimers < TIMERQUEUE_CAP);

    timerqueue_disarm(tq);
    sink(timer, tq->queue, tq->ntimers++);
    timerqueue_arm(tq);

    return timer;
}

int bt_timerqueue_init(BT_TimerQueue *tq, BT_EventLoop *eloop)
{
    tq->emitter = (BT_EventProducer){.on_read = timer_fire};
    tq->eloop = eloop;
    tq->timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
    if (tq->timerfd < 0) {
        perror("timerfd_create()");
        return -1;
    }
    tq->ntimers = 0;
    return 0;
}

int bt_timerqueue_extend(BT_TimerQueue *tq, BT_Timer timer, int incr)
{
    timerqueue_disarm(tq);

    for (size_t i=0; i<tq->ntimers; i++) {
        if (timer != tq->queue[i])
            continue;

        struct bt_timer old = *timer;
        timer->value.it_value.tv_sec += incr;

        if (tq->ntimers - i - 1) {
            pop(tq->queue + i, tq->ntimers - i);
            timer_add(tq->queue[i], &old);
            sink(timer, tq->queue+i, tq->ntimers - 1 - i);
        }

        break;
    }

    timerqueue_arm(tq);
    return 0;
}

int bt_timerqueue_remove(BT_TimerQueue *tq, BT_Timer timer)
{
    timerqueue_disarm(tq);

    for (size_t i=0; i<tq->ntimers; i++) {
        if (timer != tq->queue[i])
            continue;
        if (tq->ntimers - i - 1) {
            pop(tq->queue + i, tq->ntimers - i);
            timer_add(tq->queue[i], timer);
        }

        tq->ntimers--;
        break;
    }

    timerqueue_arm(tq);
    return 0;
}
