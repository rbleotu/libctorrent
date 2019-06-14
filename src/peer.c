#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "peer.h"
#include "message.h"
#include "piece.h"
#include "net.h"
#include "../include/torrent.h"
#include "torrentx.h"

local void
queue_push(BT_Peer peer, struct bt_msg *msg)
{
    assert (peer->nq < PEER_TXQCAP);
    if (peer->qtail >= PEER_TXQCAP)
        peer->qtail = 0;
    peer->txqueue[peer->qtail++] = msg;
    peer->nq++;
}

local struct bt_msg *
queue_pop(BT_Peer peer)
{
    assert (peer->nq != 0);
    if (peer->qhead >= PEER_TXQCAP)
        peer->qhead = 0;
    struct bt_msg * const msg = peer->txqueue[peer->qhead++];
    peer->nq--;
    return msg;
}

local bool
has_message(const byte src[], size_t sz)
{
    if (sz < 4)
        return false;
    size_t len = 0;
    GET_U32BE(src, len);
    if (sz < len+4)
        return false;
    return true;
}

local inline int
has_handshake(const byte src[], size_t len)
{
    return len >= HSHK_LEN;
}

local int
msg2event(int id)
{
    switch (id) {
    case BT_MREQUEST:
        return BT_EVPEER_REQUEST;
    case BT_MPIECE:
        return BT_EVPEER_PIECE;
    case BT_MCHOKE:
        return BT_EVPEER_CHOKE;
    case BT_MUNCHOKE:
        return BT_EVPEER_UNCHOKE;
    case BT_MINTERESTED:
        return BT_EVPEER_INTERESTED;
    case BT_MNOT_INTERESTED:
        return BT_EVPEER_NOTINTERESTED;
    case BT_MHAVE:
        return BT_EVPEER_HAVE;
    case BT_MBITFIELD:
        return BT_EVPEER_BITFIELD;
    case BT_MKEEP_ALIVE:
        return BT_EVPEER_KEEPALIVE;
    case BT_MPORT:
        return 0;
    case BT_MCANCEL:
        return 0;
    case BT_MSG_HANDSHAKE:
        return BT_EVPEER_HANDSHAKE;
    default:
        printf("%d\n", id);
        assert (!"not handled");
    }
    return 0;
}

local void
fill_event(struct bt_event *out, BT_Peer peer, struct bt_msg *msg)
{
    out->type = msg2event(msg->id);
    out->a = peer;
    out->b = msg;
}

local int
on_read(BT_EventProducer *prod, BT_EventQueue *queue)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    assert (this != NULL);

    if (!this->connected)
        return 0;

    if (!this->handshake_done) {
        assert (this->rxhave < HSHK_LEN);
        ssize_t n = net_tcp_recv(this->sockfd, this->rxbuf + this->rxhave, HSHK_LEN - this->rxhave);
        if (n < 0) {
            perror("recv()");
            return -1;
        }

        if (n == 0) {
            bt_eventqueue_push(queue, BT_EVENT(BT_EVPEER_DISCONNECT, this, NULL));
            return 1;
        }

        this->rxhave += n;
        assert (this->rxhave <= HSHK_LEN);

        if (has_handshake(this->rxbuf, this->rxhave)) {
            struct bt_handshake *hshk = bt_handshake_unpack(this->rxbuf);
            this->rxhave -= HSHK_LEN;

            bt_eventqueue_push(queue, BT_EVENT(BT_EVPEER_HANDSHAKE, this, hshk));
            return 1;
        }

        return 0;
    }

    ssize_t n = net_tcp_recv(this->sockfd, this->rxbuf + this->rxhave, sizeof(this->rxbuf) - this->rxhave);
    if (n < 0) {
        perror("recv()");
        return -1;
    }

    if (n == 0) {
        bt_eventqueue_push(queue, BT_EVENT(BT_EVPEER_DISCONNECT, this, NULL));
        return 1;
    }

    do {
        this->rxhave += n;

        while (has_message(this->rxbuf, this->rxhave)) {
            struct bt_msg *msg = bt_msg_unpack(this->rxbuf);

            memmove(this->rxbuf, this->rxbuf + bt_msg_len(msg), this->rxhave - bt_msg_len(msg));
            this->rxhave -= bt_msg_len(msg);

            bt_eventqueue_push(queue, BT_EVENT(msg2event(msg->id), this, msg));
        }

        n = net_tcp_recv(this->sockfd, this->rxbuf + this->rxhave, sizeof(this->rxbuf) - this->rxhave);
        if (n < 0) {
            perror("recv()");
            return -1;
        }
    } while(n);

    return 0;
}

local int
on_destroy(BT_EventProducer *prod, BT_EventQueue *queue)
{
    const BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    bt_eventqueue_push(queue, BT_EVENT(BT_EVPEER_DISCONNECT, this, NULL));
    return 0;
}

local int
on_write(BT_EventProducer *prod, BT_EventQueue *queue)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    size_t n;

    if (!this->connected) {
        if (net_tcp_haserror(this->sockfd)) {
            perror("net_tcp_haserror");
            return -1;
        }

        bt_eventqueue_push(queue, BT_EVENT(BT_EVPEER_CONNECT, this, NULL));
        return 1;
    }

    do {
        if (!this->txrem) {
            if (this->nq) {
                struct bt_msg *msg = queue_pop(this);
                bt_msg_pack(this->txbuf, msg);

                this->txrem = bt_msg_len(msg);
                this->txnext = this->txbuf;

                bt_msg_free(msg);
            } else {
                return 0;
            }
        }

        n = net_tcp_send(this->sockfd, this->txnext, this->txrem);
        if (n < 0) {
            return -1;
        }

        this->txnext += n;
        this->txrem -= n;
    } while (n);

    return 0;
}

int bt_peer_init(BT_Peer peer, BT_EventLoop *eloop, size_t npieces)
{
    assert (peer);
    *peer = PEER_INIT();

    peer->eloop = eloop;
    peer->pieces = bt_bitset_new(npieces);
    if (!peer->pieces) {
        return -1;
    }

    return 0;
}

int bt_peer_connect(BT_Peer peer, uint32 ipv4, uint16 port)
{
    assert (peer);
    assert (peer->eloop);

    const int sockfd = net_tcp_connect(ipv4, port);
    if (sockfd < 0) {
        perror("connect");
        return 0;
    }

    peer->sockfd = sockfd;
    peer->evproducer = (BT_EventProducer) {
        .on_write = on_write,
        .on_read = on_read,
        .on_destroy = on_destroy,
    };
    peer->ipv4 = ipv4;
    peer->port = port;

    bt_eventloop_register(peer->eloop, sockfd, &peer->evproducer);
    return 0;
}

int bt_peer_choke(BT_Peer peer)
{
    if (peer->am_choking)
        return 0;

    peer->am_choking = true;
    struct bt_msg *choke = bt_msg_new(BT_MCHOKE);
    if (!choke) {
        perror("alloc");
        bt_errno = BT_EALLOC;
        return -1;
    }
    queue_push(peer, choke);
    return 0;
}

int bt_peer_unchoke(BT_Peer peer)
{
    if (!peer->am_choking)
        return 0;

    peer->am_choking = false;
    struct bt_msg *unchoke = bt_msg_new(BT_MUNCHOKE);
    if (!unchoke) {
        perror("alloc");
        bt_errno = BT_EALLOC;
        return -1;
    }
    queue_push(peer, unchoke);
    return 0;
}

int bt_peer_interested(BT_Peer peer)
{
    if (peer->am_interested)
        return 0;

    peer->am_interested = true;
    struct bt_msg *interested = bt_msg_new(BT_MINTERESTED);
    if (!interested) {
        perror("alloc");
        bt_errno = BT_EALLOC;
        return -1;
    }

    queue_push(peer, interested);
    return 0;
}

int bt_peer_have(BT_Peer peer, uint32 id)
{
    struct bt_msg *have = bt_msg_new(BT_MHAVE, id);
    if (!have) {
        perror("alloc");
        bt_errno = BT_EALLOC;
        return -1;
    }

    queue_push(peer, have);
    return 0;
}

int bt_peer_notinterested(BT_Peer peer)
{
    if (!peer->am_interested)
        return 0;

    peer->am_interested = false;
    struct bt_msg *ninterested = bt_msg_new(BT_MNOT_INTERESTED);
    if (!ninterested) {
        perror("alloc");
        bt_errno = BT_EALLOC;
        return -1;
    }

    queue_push(peer, ninterested);
    return 0;
}

int bt_peer_disconnect(BT_Peer peer)
{
    if (peer->eloop) {
        bt_eventloop_unregister(peer->eloop, peer->sockfd, &peer->evproducer);
        peer->eloop = NULL;
    }

    if (peer->sockfd != -1) {
        net_tcp_disconnect(peer->sockfd);
        peer->sockfd = -1;
    }

    return 0;
}

int bt_peer_requestpiece(BT_Peer peer, uint32 pieceid, uint32 start, uint32 len)
{
    printf("requesting %u [%u, %u]\n", pieceid, start, len);

    struct bt_msg *req = bt_msg_new(BT_MREQUEST, pieceid, start, len);
    if (!req) {
        perror("alloc");
        bt_errno = BT_EALLOC;
        return -1;
    }

    queue_push(peer, req);
    return 0;
}

int bt_peer_sendpiece(BT_Peer peer, byte buf[], uint32 pieceid, uint32 off, uint32 len)
{
    peer->downloaded += len;

    return 0;
}

int bt_peer_recvmsg(BT_Peer peer)
{
    return 0;
}

int bt_peer_handshake(BT_Peer peer, struct bt_handshake *hshk)
{
    queue_push(peer, (void *)hshk);
    return 0;
}

BT_Peer bt_peer_fromsock(BT_EventLoop *eloop, int sockfd, size_t npieces)
{
    BT_Peer peer = malloc(sizeof(*peer));
    if (!peer) {
        perror("malloc");
        return NULL;
    }
    *peer = PEER_INIT();

    peer->eloop = eloop;
    peer->sockfd = sockfd;
    peer->pieces = bt_bitset_new(npieces);
    if (!peer->pieces) {
        perror("malloc");
        bt_errno = BT_EALLOC;
        free(peer);
        return NULL;
    }

    peer->evproducer = (BT_EventProducer) {
        .on_write = on_write,
        .on_read = on_read,
        .on_destroy = on_destroy,
    };

    bt_eventloop_register(eloop, sockfd, &peer->evproducer);
    return peer;
}

local bool
valid_handshake(BT_Torrent t)
{
    return true;
}

local int
peer_onconnect(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);
    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] connected\n", ip[0], ip[1], ip[2], ip[3]);

    peer->connected = true;

    byte peer_id[20] = "hello world";
    struct bt_handshake *hshk = bt_handshake(t->info_hash, peer_id);
    bt_peer_handshake(peer, hshk);

    return 0;
}

local int
peer_onhandshake(BT_Torrent t, BT_Peer peer, struct bt_handshake *hshk)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);
    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] handshake\n", ip[0], ip[1], ip[2], ip[3]);

    if (!valid_handshake(t)) {
        bt_peer_disconnect(peer);
        return 0;
    }

    peer->handshake_done = true;
    puts("... handshake OK!");

    bt_peer_interested(peer);
    return 0;
}

local int
peer_oninterested(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] interested\n", ip[0], ip[1], ip[2], ip[3]);
    peer->peer_interested = true;
    return 0;
}

local inline int
peer_onnotinterested(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] not interested\n", ip[0], ip[1], ip[2], ip[3]);
    peer->peer_interested = false;
    return 0;
}

local inline int
peer_onchoke(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] choke\n", ip[0], ip[1], ip[2], ip[3]);
    peer->peer_choking = true;
    return 0;
}

local inline int
peer_onunchoke(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] unchoke\n", ip[0], ip[1], ip[2], ip[3]);
    peer->peer_choking = false;

    return 0;
}

local int
peer_onkeepalive(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] keepalive\n", ip[0], ip[1], ip[2], ip[3]);
    return 0;
}

local int
peer_onhave(BT_Torrent t, BT_Peer peer, size_t piecei)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] have %zu\n", ip[0], ip[1], ip[2], ip[3], piecei);

    bt_bitset_set(peer->pieces, piecei);

    BT_Piece piece = bt_torrent_get_piece(t, piecei);
    bt_piece_incrfreq(piece);

    return 0;
}

local int
peer_ondisconnect(BT_Torrent t, BT_Peer peer)
{
    if (peer->connected) {
        byte ip[4];
        PUT_U32BE(ip, peer->ipv4);

        printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] disconnect\n", ip[0], ip[1], ip[2], ip[3]);
    }

    if (peer->handshake_done) {
        for (size_t i=0; i<t->npieces; i++) {
            if (bt_bitset_get(peer->pieces, i)) {
                BT_Piece piece = bt_torrent_get_piece(t, i);
                bt_piece_decrfreq(piece);
            }
        }
    }

    bt_peer_disconnect(peer);
    return 0;
}

local int
peer_onbitfield(BT_Torrent t, BT_Peer peer, byte set[], size_t n)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] bitfield\n", ip[0], ip[1], ip[2], ip[3]);

    bt_bitset_read_arr(peer->pieces, set, n);
    for (size_t i=0; i<t->npieces; i++) {
        if (bt_bitset_get(peer->pieces, i)) {
            BT_Piece piece = bt_torrent_get_piece(t, i);
            bt_piece_incrfreq(piece);
        }
    }

    return 0;
}

local int
peer_onpiece(BT_Torrent t, BT_Peer peer, const byte data[], uint32 index, uint32 off, uint32 len)
{
    printf("got piece: %u %u %u\n", index, off, len);

    BT_Piece piece = bt_torrent_get_piece(t, index);
    bt_piece_incrdlrover(piece, len);

    peer->uploaded += len;

    if (bt_piece_complete(piece)) {
        bt_torrent_piece_completed(t, piece);
    }

    //byte *buf = bt_piececache_alloc(piece, off, len);
    //if (!buf) {
    //    perror("cache read()");
    //    return -1;
    //}

    //memcpy(buf, data, len);
    //bt_piececache_markdirty(piece, off, len);

    //if (bt_piececache_complete(piece)) {
    //    bt_piececache_flush(piece);
    //}
}

local int
peer_onrequest(BT_Torrent t, BT_Peer peer, uint32 id, uint32 off, uint32 len)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);

    printf("[\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m] REQUEST\n", ip[0], ip[1], ip[2], ip[3]);
}

local int
dispatch(int ev, BT_Torrent t, BT_Peer peer, void *data)
{
    switch (ev) {
        HANDLE_EVENT(t, peer, EVPEER_CONNECT, peer_onconnect);
        HANDLE_EVENT(t, peer, EVPEER_HANDSHAKE, peer_onhandshake);
        HANDLE_EVENT(t, peer, EVPEER_DISCONNECT, peer_ondisconnect);
        HANDLE_EVENT(t, peer, EVPEER_HAVE, peer_onhave);
        HANDLE_EVENT(t, peer, EVPEER_INTERESTED, peer_oninterested);
        HANDLE_EVENT(t, peer, EVPEER_NOTINTERESTED, peer_onnotinterested);
        HANDLE_EVENT(t, peer, EVPEER_UNCHOKE, peer_onunchoke);
        HANDLE_EVENT(t, peer, EVPEER_KEEPALIVE, peer_onkeepalive);
        HANDLE_EVENT(t, peer, EVPEER_BITFIELD, peer_onbitfield);
        HANDLE_EVENT(t, peer, EVPEER_PIECE, peer_onpiece);
    }
    return 0;
}

int
bt_peer_handlemessage(BT_Torrent t, BT_Peer peer, int ev, void *data)
{
    assert (t != NULL);
    assert (peer != NULL);

    const int ec = dispatch(ev, t, peer, data);
    if (data)
        bt_msg_free(data);

    return ec;
}

double bt_peer_progress(BT_Torrent t, BT_Peer peer)
{
    const size_t n = t->npieces;
    uint64 have = 0;

    for (size_t i=0; i<n; i++) {
        if (bt_bitset_get(peer->pieces, i)) {
            have += t->piece_length;
        }
    }

    return (double)have / t->size;
}

bool
bt_peer_haspiece(BT_Peer peer, uint32 piecei)
{
    return bt_bitset_get(peer->pieces, piecei);
}

uint32 bt_peer_ulrate(BT_Peer peer)
{
    if (!peer)
        return 0;
    return peer->uploaded;
}

uint32 bt_peer_dlrate(BT_Peer peer)
{
    if (!peer)
        return 0;
    return peer->downloaded;
}

int bt_peer_requestpiecefull(BT_Peer peer, BT_Piece piece, uint32 id)
{
    bt_piece_mark_downloading(piece);
    size_t off = piece->dlnext;

    while (off < piece->length) {
        uint32 n = MIN(1<<14, piece->length - off);
        bt_peer_requestpiece(peer, id, off, n);
        off += n;
    }
}
