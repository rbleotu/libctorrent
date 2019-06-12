#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "peer.h"
#include "net.h"

local int
on_read(BT_EventProducer *prod)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    assert (this != NULL);

    char buf[1<<10];

    const ssize_t n = net_tcp_recv(this->sockfd, buf, sizeof(buf));
    if (n < 0) {
        perror("recv");
        return -1;
    }

    puts("recv()");
    return 0;
}

local int
on_write(BT_EventProducer *prod)
{
    return 0;
}

local int
on_connect(BT_EventProducer *prod)
{
    BT_Peer this = container_of(prod, struct bt_peer, evproducer);
    prod->on_read = on_read;
    this->connected = true;

    puts("connected");
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
    peer->evproducer =
        (BT_EventProducer) {.on_write = on_write, .on_read = on_connect};

    bt_eventloop_register(peer->eloop, sockfd, &peer->evproducer);
    return 0;
}
