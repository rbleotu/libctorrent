#pragma once

#define EVENT_INIT()       \
    ((struct bt_event) {   \
         .type = BT_EVNONE,\
         .a    = NULL,     \
         .b    = NULL,     \
     })

enum {
    BT_EVNONE,

    BT_EVPIECE_COMPLETE,

    BT_EVPEER_CONNECT,
    BT_EVPEER_HANDSHAKE,
    BT_EVPEER_REQUEST,
    BT_EVPEER_PIECE,
    BT_EVPEER_CHOKE,
    BT_EVPEER_UNCHOKE,
    BT_EVPEER_INTERESTED,
    BT_EVPEER_NOTINTERESTED,
    BT_EVPEER_HAVE,
    BT_EVPEER_BITFIELD,
    BT_EVPEER_DISCONNECT,
    BT_EVPEER_KEEPALIVE,
    BT_EVPEER_REQTIMEOUT,
    BT_EVPEER_RUNSCHEDULER,

    BT_EVSEND_KEEPALIVE,

    BT_EVPAUSE,

    BT_EVENT_COUNT,
};

typedef struct bt_event BT_Event;

struct bt_event {
    int type;

    //satellite data
    void *a, *b;
};

#define BT_EVENT(t,a,b) \
    ((struct bt_event){(t), (a), (b)})
