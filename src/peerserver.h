#pragma once

#include "eventloop.h"

typedef struct bt_peerserver BT_PeerServer;

struct bt_peerserver {
    int sockfd;
    uint16 port;
    struct bt_eventloop *eloop;
    struct bt_eventproducer emitter;
};

#define PEERSERVER_INIT()       \
    ((struct bt_peerserver) {   \
        .sockfd = -1,           \
        .port = 8080,           \
        .eloop = NULL,          \
        .emitter = {NULL, NULL},\
    })

int bt_peerserver_init(struct bt_peerserver *server, BT_EventLoop *eloop, uint16 port);
int bt_peerserver_listen(struct bt_peerserver *server);
int bt_peerserver_destroy(struct bt_peerserver *server);
