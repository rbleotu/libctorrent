#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "peerserver.h"
#include "message.h"
#include "net.h"
#include "peer.h"
#include "../include/torrent.h"

int on_accept(BT_EventProducer *prod, BT_EventQueue *queue)
{
    BT_PeerServer *this = container_of(prod, struct bt_peerserver, emitter);

    const int clientfd = net_tcp_accept(this->sockfd);
    if (clientfd < 0) {
        perror("accept()");
        return -1;
    }

    BT_Peer peer = bt_peer_fromsock(this->eloop, clientfd, 100);
    if (!peer) {
        return -1;
    }

    bt_eventqueue_push(queue, BT_EVENT(BT_EVPEER_CONNECT, peer, NULL));
}

int bt_peerserver_init(struct bt_peerserver *server, BT_EventLoop *eloop, uint16 port)
{
    *server = PEERSERVER_INIT();
    server->port = port;
    server->eloop = eloop;
}

int bt_peerserver_listen(struct bt_peerserver *server)
{
    server->sockfd = net_tcp_ipv4listen(server->port);
    if (server->sockfd < 0) {
        perror("listen()");
        return -1;
    }

    server->emitter = (struct bt_eventproducer) {
        .on_write = NULL,
        .on_read = NULL,
        .on_destroy = NULL,
    };

    bt_eventloop_register(server->eloop, server->sockfd, &server->emitter);
    if (net_tcp_accept(server->sockfd)) {
        if (errno != EWOULDBLOCK) {
            perror("accept()");
            return -1;
        }
    }
    return 0;
}


