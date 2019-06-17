#ifndef PEER_H_
#define PEER_H_

#include "util/common.h"
#include "util/vector.h"
#include "bitset.h"
#include "pqueue.h"
#include "eventloop.h"
#include "message.h"
#include "piece.h"
#include "timer_queue.h"
#include "../include/torrent.h"
#include "txbuf.h"

#define PEER_TXQCAP 4096
#define PEER_REQCAP 2048
#define PEER_KEEPALIVE_TIMEOUT 120
#define PEER_KEEPALIVE_RATE     40

VECTOR_DEFINE(BT_Piece, PieceRef);

typedef struct bt_peer *BT_Peer;
#define REQUEST_COOLDOWN 5

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

    uint64 uploaded;
    uint64 downloaded;

    uint64 ulrate;
    uint64 dlrate;

    time_t last_request;
    time_t connect_time;
    BT_PQueue requestq;

    BT_Bitset pieces;
    Vector(PieceRef) pieceshas;

    uint64 request_queue[PEER_REQCAP];
    size_t nrequests;

    struct bt_timer request_timeout;
    struct bt_timer keepalive_timeout;
    struct bt_timer cooldown_timer;

    BT_EventLoop *eloop;

    struct bt_eventproducer evproducer;

    byte rxbuf[1<<20];
    int rxhave;

    struct txbuf txbuf;

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
        .pieceshas       = VECTOR(),    \
        .uploaded        = 0,           \
        .downloaded      = 0,           \
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
int bt_peer_have(BT_Peer peer, uint32 piece);
int bt_peer_requestpiece(BT_Peer peer, uint32 pieceid, uint32 start, uint32 len);
int bt_peer_sendpiece(BT_Peer, byte data[], uint32 pieceid, uint32 start, uint32 len);
int bt_peer_recvmsg(BT_Peer peer);
int bt_peer_handlemessage(BT_Torrent t, BT_Peer peer, int msgtype, void *data);
int bt_peer_keepalive(BT_Peer peer);
double bt_peer_progress(BT_Torrent t, BT_Peer peer);
bool bt_peer_haspiece(BT_Peer peer, uint32 piecei);
double bt_peer_ulrate(BT_Peer peer);
double bt_peer_dlrate(BT_Peer peer);
int bt_peer_updaterequests(BT_Torrent t, BT_Peer peer);
void bt_peer_flushreqqueue(BT_Torrent t, BT_Peer peer);


#endif
