#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "peer.h"
#include "message.h"
#include "net.h"
#include "../include/torrent.h"

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
    size_t len;
    GET_U32BE(src, len);
    if (sz < len+4)
        return false;
    return true;
}

local int
has_handshake(const byte src[], size_t len)
{
    return false;
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
    }
}

local void
fill_event(struct bt_event *out, BT_Peer peer, struct bt_msg *msg)
{
    out->type = msg2event(msg->id);
    out->a = peer;
    out->b = msg;
}

local int
on_read(BT_EventProducer *prod, struct bt_event *out)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    assert (this != NULL);

    if (!this->connected)
        return 0;

    ssize_t n;

    do {
        n = net_tcp_recv(this->sockfd, this->rxbuf + this->rxhave, sizeof(this->rxbuf) - this->rxhave);
        if (n < 0) {
            perror("recv()");
            return -1;
        }

        this->rxhave += n;

        if (this->handshake_done) {
            if (has_message(this->rxbuf, this->rxhave)) {
                struct bt_msg *msg = bt_msg_unpack(this->rxbuf);
                memmove(this->rxbuf, this->rxbuf + bt_msg_len(msg), bt_msg_len(msg));
                this->rxhave -= bt_msg_len(msg);
                fill_event(out, this, msg);
                return msg->id;
            }
        } else {
            if (has_handshake(this->rxbuf, this->rxhave)) {
                //


            }
        }
    } while (n);

    return 0;
}

local int
on_destroy(BT_EventProducer *prod)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    this->connected = false;
    if (this->sockfd != -1) {
        net_tcp_disconnect(this->sockfd);
        this->sockfd = -1;
    }
    return BT_EVPEER_DISCONNECT;
}

local int
on_write(BT_EventProducer *prod, struct bt_event *out)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    size_t n;

    if (!this->connected) {
        if (net_tcp_haserror(this->sockfd)) {
            return -1;
        }
        this->connected = true;
        out->a = this;
        return out->type = BT_EVPEER_CONNECT;
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

    bt_eventloop_register(peer->eloop, sockfd, &peer->evproducer);
    return 0;
}

int bt_peer_choke(BT_Peer peer)
{
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
    return 0;
}

int bt_peer_requestpiece(BT_Peer peer, uint32 pieceid, uint32 start, uint32 len)
{
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
