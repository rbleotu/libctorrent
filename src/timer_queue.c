#include <signal.h>
#include <sys/time.h>
#include <pthread.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "util/error.h"
#include "util/common.h"
#include "timer_queue.h"

local volatile sig_atomic_t got_alarm;

struct bt_tqueue {
    pthread_mutex_t lock;
    volatile size_t n;
    size_t cap;

    struct bt_timer v[];
};

local int pipefd[2];

BT_TQueue
bt_tqueue_new(size_t cap)
{
    BT_TQueue tq = malloc(sizeof(*tq) + sizeof(struct bt_timer[cap]));
    if (!tq) {
        bt_errno = BT_EALLOC;
        return NULL;
    }
    tq->n = 0, tq->cap = cap;
    pthread_mutex_init(&tq->lock, NULL);
    return tq;
}

local inline unsigned
timer_get(void)
{
    struct itimerval tmval;
    getitimer(ITIMER_REAL, &tmval);
    return tmval.it_value.tv_usec / 500000UL + tmval.it_value.tv_sec;
}

local void
alarm_handler(int sig)
{
    uint8 x = 1;
    got_alarm = 1;
    write(pipefd[1], &x, 1);
}

local inline int
timer_disable(void)
{
    struct sigaction sa = {.sa_handler = SIG_IGN, .sa_flags = 0};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        return -1;
    struct itimerval tmval = {{0, 0}, {0, 0}};
    return setitimer(ITIMER_REAL, &tmval, NULL);
}

local int
timer_enable(struct bt_timer tm)
{
    struct sigaction sa = {.sa_handler = alarm_handler,
                           .sa_flags = 0};
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGALRM, &sa, NULL) == -1)
        return -1;
    struct itimerval tmval = {{0, 0}, {tm.nsec, 1}};
    return setitimer(ITIMER_REAL, &tmval, NULL);
}

local void
sink(struct bt_timer v[], size_t n, struct bt_timer tm)
{
    while (n--) {
        if (v[n].nsec > tm.nsec) {
            v[n].nsec -= tm.nsec;
            break;
        } else {
            tm.nsec -= v[n].nsec;
            v[n + 1] = v[n];
        }
    }
    v[n + 1] = tm;
}

void
bt_tqueue_fprint(BT_TQueue tq)
{
    putc('[', stdout);
    for (size_t i = tq->n; i--;) {
        printf("%u ", tq->v[i].nsec);
    }
    puts("]");
}

int
bt_tqueue_insert(BT_TQueue tq, struct bt_timer tm)
{
    assert(tq);
    if (tq->n >= tq->cap)
        return -1;

    pthread_mutex_lock(&tq->lock);
    {
        size_t n = tq->n++;
        if (n)
            tq->v[n - 1].nsec = timer_get();

        timer_disable();
        sink(tq->v, n, tm);
        timer_enable(tq->v[n]);
    }
    pthread_mutex_unlock(&tq->lock);

    return 0;
}

struct bt_timer
bt_tqueue_remove(BT_TQueue tq)
{
    struct bt_timer tm;
    assert(tq);
    timer_disable();
    pthread_mutex_lock(&tq->lock);
    tm = tq->v[--tq->n];
    pthread_mutex_unlock(&tq->lock);
    return tm;
}

void *
bt_timerthread(BT_TQueue q)
{
    assert(q);
    uint8 x;
    if (pipe(pipefd) < 0)
        return NULL;
    while (1) {
        pause();
        if (got_alarm) {
            pthread_mutex_lock(&q->lock);
            timer_disable();
            struct bt_timer tm = q->v[--q->n];
            tm.handler(tm.data);
            got_alarm = 0;
            if (q->n)
                timer_enable(q->v[q->n - 1]);
            pthread_mutex_unlock(&q->lock);
        }
    }
    return NULL;
}
