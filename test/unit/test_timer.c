#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "timer_queue.h"

enum {
    EVHELLO_WORLD,
    EVSTOP,
};

int main(void)
{
    BT_EventLoop loop;
    BT_EventQueue queue;
    BT_TimerQueue tqueue;

    bt_eventloop_init(&loop);
    bt_eventqueue_init(&queue);
    bt_timerqueue_init(&tqueue, &loop);

    struct bt_timer timer1 = TIMER(2, BT_EVENT(EVHELLO_WORLD, NULL, NULL));
    struct bt_timer timer2 = TIMER(5, BT_EVENT(EVSTOP, NULL, NULL));

    bt_timerqueue_insert(&tqueue, &timer1);
    bt_timerqueue_insert(&tqueue, &timer2);

    bool running = true;
    int i = 0;

    while (running) {
        bt_eventloop_run(&queue, &loop);

        while (!bt_eventqueue_isempty(&queue)) {
            switch(bt_eventqueue_pop(&queue, NULL)) {
            case EVHELLO_WORLD:
                if (!i) {
                    puts("hello world");
                    timer1 = TIMER(1, BT_EVENT(EVHELLO_WORLD, NULL, NULL));
                    bt_timerqueue_insert(&tqueue, &timer1);
                } else if (i == 1) {
                    puts("hello again");
                    timer1 = TIMER(5, BT_EVENT(EVHELLO_WORLD, NULL, NULL));
                    bt_timerqueue_insert(&tqueue, &timer1);
                    bt_timerqueue_extend(&tqueue, &timer2, 5);
                } else {
                    puts("goodbye in any moment");
                }
                i = i + 1;
                break;
            case EVSTOP:
                puts("stopping...");
                running = false;
                break;
            }
        }
    }

    return 0;
}
