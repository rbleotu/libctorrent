#ifndef PEER_H_
#define PEER_H_

#include "util/common.h"
#include "bitset.h"
#include "pqueue.h"
#include "eventloop.h"
#include "message.h"
#include "../include/torrent.h"

#define PEER_TXQCAP 128

typedef struct bt_peer *BT_Peer;

struct bt_peer {
    int sockfd;

    uint32 ipv4;
    uint32 port;

    bool connected;
    bool handshake_done;

    int am_choking;
    int am_interested;

    int peer_choking;
    int peer_interested;

    BT_PQueue requestq;

    BT_Bitset pieces;

    BT_EventLoop *eloop;

    struct bt_eventproducer evproducer;

    byte rxbuf[17<<10];
    int rxhave;

    byte txbuf[17<<10];
    byte *txnext;
    int txrem;

    struct bt_msg *txqueue[PEER_TXQCAP];
    int qhead, qtail;
    int nq;
};

#define PEER_INIT() ((struct bt_peer) { \
        .evproducer      = {NULL, NULL},\
        .sockfd          = -1,          \
        .connected       = false,       \
        .handshake_done  = false,       \
        .ipv4            = 0,           \
        .port            = 0,           \
        .am_choking      = true,        \
        .am_interested   = false,       \
        .peer_choking    = true,        \
        .peer_interested = false,       \
        .requestq        = NULL,        \
        .pieces          = NULL,        \
    })

int bt_peer_init(BT_Peer peer, BT_EventLoop *eloop, size_t npieces);
BT_Peer bt_peer_fromsock(BT_EventLoop *eloop, int sockfd, size_t npieces);
int bt_peer_connect(BT_Peer peer, uint32 ipv4, uint16 port);
int bt_peer_handshake(BT_Peer peer, struct bt_handshake *hshk);
int bt_peer_disconnect(BT_Peer peer);
int bt_peer_choke(BT_Peer peer);
int bt_peer_unchoke(BT_Peer peer);
int bt_peer_interested(BT_Peer peer);
int bt_peer_notinterested(BT_Peer peer);
int bt_peer_requestpiece(BT_Peer peer, uint32 pieceid, uint32 start, uint32 len);
int bt_peer_sendpiece(BT_Peer, byte data[], uint32 pieceid, uint32 start, uint32 len);
int bt_peer_recvmsg(BT_Peer peer);
int bt_peer_handlemessage(BT_Torrent t, BT_Peer peer, int msgtype, void *data);

#endif
