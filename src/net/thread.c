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

local void
fill_queue(BT_PQueue q, BT_Bitset peer, struct bt_piece own[],
           size_t n)
{
    while (n--) {
        if (!bt_bitset_get(peer, n))
            continue;
        own[n].freq++;
        if (!own[n].have)
            assert(bt_pqueue_insert(q, n, own[n].freq) != -1);
    }
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
    p->requestq = bt_pqueue_new(t->npieces);
    if (!p->requestq)
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
            puts("got unchoked");
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
            fill_queue(p->requestq, p->pieces, t->piecetab,
                       t->npieces);
        } break;
        case BT_MHAVE: {
            struct bt_msg_have *have = (void *)msg;
            size_t i = have->piecei;
            int freq = ++t->piecetab[i].freq;
            bt_bitset_set(p->pieces, i);
            if (!t->piecetab[i].have)
                assert(bt_pqueue_insert(p->requestq, i, freq) != -1);
        } break;
        case BT_MREQUEST:
            break;
        case BT_MPIECE: {
            struct bt_msg_piece *mpiece = (void *)msg;
            u32 pi = mpiece->piecei;
            u32 ci = mpiece->begin / CHUNK_SZ;
            if (mpiece->len - 9 != CHUNK_SZ) {
                puts("chunk size not matching request size");
                goto cleanup;
            }

            bt_piece_fprint(stdout, &t->piecetab[pi]);
            if (bt_piece_add_chunk(&t->piecetab[pi], ci,
                                   mpiece->block) < 0) {
                puts("error allocating piece");
                return NULL;
            }
            if (t->piecetab[pi].have) {
                if (bt_piece_check(&t->piecetab[pi])) {
                    bt_pqueue_remove(p->requestq);
                    t->piecetab[pi].freq++;
                    if (bt_disk_write_piece(t->piecetab[pi].data,
                                            t->mgr,
                                            &t->piecetab[pi]) < 0) {
                        puts("error writing piece to disk");
                    }
                } else {
                    puts("bad hash !");
                }
            }
        } break;
        default:
            assert(0);
        }
        if (!pq_isempty(p->requestq)) {
            if (!p->am_interested) {
                p->am_interested = 1;
                bt_msg_send(p->sockfd, BT_MINTERESTED);
            } else if (!p->peer_choking) {

                unsigned piecei = pq_min(p->requestq);
                BT_Piece piece = &t->piecetab[piecei];
                size_t off = bt_piece_empty_chunk(piece) * CHUNK_SZ;
                size_t len = MIN(off + CHUNK_SZ, piece->length) - off;

                if (off >= piece->length) {
                    bt_pqueue_remove(p->requestq);
                } else {
                    bt_msg_send(p->sockfd, BT_MREQUEST, piecei,
                                (unsigned)off, (unsigned)len);
                    printf("request for piece %u\n", piecei);
                }
            }
        } else if (p->am_interested) {
            p->am_interested = 0;
            bt_msg_send(p->sockfd, BT_MNOT_INTERESTED);
        }
        bt_msg_free(msg);
    }

cleanup:
    close(p->sockfd);
    free(p->pieces);
    free(p->requestq);
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
