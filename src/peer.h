#ifndef PEER_H_
#define PEER_H_

#include "util/common.h"
#include "bitset.h"
#include "pqueue.h"
#include "eventloop.h"

typedef struct bt_peer *BT_Peer;

struct bt_peer {
    int sockfd;

    bool connected;

    int am_choking;
    int am_interested;

    int peer_choking;
    int peer_interested;

    BT_PQueue requestq;

    BT_Bitset pieces;

    BT_EventLoop *eloop;

    struct bt_eventproducer evproducer;
};

#define PEER_INIT() ((struct bt_peer) { \
        .evproducer      = {NULL, NULL},\
        .sockfd          = -1,          \
        .connected       = false,       \
        .am_choking      = true,        \
        .am_interested   = false,       \
        .peer_choking    = true,        \
        .peer_interested = false,       \
        .requestq        = NULL,        \
        .pieces          = NULL,        \
    })

int bt_peer_init(BT_Peer peer, BT_EventLoop *eloop, size_t npieces);
int bt_peer_connect(BT_Peer peer, uint32 ipv4, uint16 port);
int bt_peer_disconnect(BT_Peer peer);
int bt_peer_choke(BT_Peer peer);
int bt_peer_unchoke(BT_Peer peer);
int bt_peer_interested(BT_Peer peer);
int bt_peer_notinterested(BT_Peer peer);
int bt_peer_requestpiece(BT_Peer peer, size_t pieceid, size_t start, size_t len);

#endif
