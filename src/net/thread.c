#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <pthread.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>

#include "../common.h"
#include "../error.h"
#include "../torrentx.h"
#include "../peer.h"
#include "../message.h"
#include "../piece.h"

struct thread_data {
    BT_Torrent t;
    size_t peeri;
};

local int
peer_connect(int sockfd, struct bt_peer_addr addr)
{
    struct sockaddr_in peer_addr;

    peer_addr.sin_family = AF_INET;
    peer_addr.sin_port = htons(addr.port);
    peer_addr.sin_addr = (struct in_addr){htonl(addr.ipv4)};

    return connect(sockfd, (void *)&peer_addr, sizeof(peer_addr));
}

local BT_Peer
peer_new(struct bt_peer_addr addr)
{
    BT_Peer p = bt_malloc(sizeof(*p));
    if (!p) {
        bt_errno = BT_EALLOC;
        return NULL;
    }
    p->am_choking = 1;
    p->am_interested = 0;
    p->peer_choking = 1;
    p->peer_interested = 0;
    p->sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (p->sockfd < 0) {
        perror("socket()");
        goto cleanup;
    }
    if (peer_connect(p->sockfd, addr) < 0) {
        perror("connect()");
        goto cleanup;
    }

    return p;
cleanup:
    free(p);
    return NULL;
}

local int
interested_test(BT_Bitset peer, struct bt_piece own[], size_t n)
{
    while (n--)
        if (bt_bitset_get(peer, n) && !own[n].have)
            return 1;
    return 0;
}

local void *
thread_func(struct thread_data *tdata)
{
    BT_Torrent t = tdata->t;
    BT_Peer p = peer_new(t->addrtab[tdata->peeri]);
    if (!p) {
        perror("peer_new()");
        return NULL;
    }

    p->pieces = bt_bitset_new(t->npieces);
    if (!p->pieces)
        goto cleanup;

    struct bt_msg_handshake hshk;
    HSHK_FILL(hshk, t->info_hash, t->info_hash);

    puts("sending handshake");

    if (bt_handshake_send(&hshk, p->sockfd) < 0) {
        puts("error sending handshake");
        goto cleanup;
    }
    if (bt_handshake_recv(&hshk, p->sockfd) < 0) {
        puts("error receiving handshake");
        goto cleanup;
    }
    if (memcmp(hshk.info_hash, t->info_hash, sizeof(t->info_hash))) {
        puts("bad hash");
        goto cleanup;
    }

    struct bt_msg *msg;

    while ((msg = bt_msg_recv(p->sockfd))) {
        printf("msg id:%u\n", msg->id);
        switch (msg->id) {
        case BT_MKEEP_ALIVE:
            break;
        case BT_MCHOKE:
            p->peer_choking = 1;
            break;
        case BT_MINTERESTED:
            p->peer_interested = 1;
            break;
        case BT_MUNCHOKE:
            p->peer_choking = 0;
            break;
        case BT_MNOT_INTERESTED:
            p->peer_interested = 0;
            break;
        case BT_MBITFIELD: {
            struct bt_msg_bitfield *bfield = (void *)msg;
            if (bfield->len - 1 != CEIL_DIV(t->npieces, 8)) {
                puts("bitfield not matching number of pieces");
                goto cleanup;
            }
            bt_bitset_read_arr(p->pieces, bfield->bitfield,
                               bfield->len - 1);
        } break;
        case BT_MHAVE: {
            struct bt_msg_have *have = (void *)msg;
            bt_bitset_set(p->pieces, have->piecei);
        } break;
        case BT_MREQUEST:
            break;
        case BT_MPIECE:
            break;

        default:
            assert(0);
        }
        if (msg->id == BT_MBITFIELD || msg->id == BT_MHAVE) {
            if (!p->am_interested) {
                if (interested_test(p->pieces, t->piecetab,
                                    t->npieces)) {
                    p->am_interested = 1;
                    puts("interested!");
                    // bt_msg_send(p->sockfd, BT_MINTERESTED);
                }
            }
        }
        bt_msg_free(msg);
    }

cleanup:
    close(p->sockfd);
    free(p->pieces);
    free(p);
    return NULL;
}

int
bt_net_start(BT_Torrent t)
{
    assert(t);
    unsigned nthreads = t->naddr;
    struct thread_data tdatatab[nthreads];
    pthread_t tidtab[nthreads];
    for (unsigned i = 0; i < nthreads; i++) {
        tdatatab[i] = (struct thread_data){t, i};
        int err = pthread_create(&tidtab[i], NULL,
                                 (void *(*)(void *))thread_func,
                                 &tdatatab[i]);
        if (err != 0) {
            perror("pthread_create()");
        }
    }

    for (unsigned i = 0; i < nthreads; i++)
        pthread_join(tidtab[i], NULL);

    return 0;
}
