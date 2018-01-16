#define _GNU_SOURCE
#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <poll.h>
#include <pthread.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include <errno.h>

#include "../common.h"
#include "../error.h"
#include "../torrentx.h"
#include "../peer.h"
#include "../message.h"
#include "../piece.h"
#include "thread_msg.h"
#include "timer_queue.h"

#define KEEPALIVE_IVAL 4

enum {
    ST_MSG_INIT,
    ST_MSG_HEADER,
    ST_MSG_BODY,

    STATE_CNT
};

struct thread_data {
    size_t id;
    unsigned nclients;

    BT_Torrent t;

    BT_TQueue tq;

    u8 *mbuf;

    int rd_state;
    size_t rem_rd;
    u8 *rd_next;

    int wr_state;
    size_t rem_wr;
    u8 *wr_next;

    int mq_in;
    int (*mq_out)[2];
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
            assert(bt_pqueue_insert(q, n, rand() % 1024) != -1);
    }
}

local void
keepalive(int *pipe)
{
    puts("keepalive");
    bt_thmsg_send(*pipe, KEEPALIVE);
}

local int
control_machine(BT_Torrent t, BT_Peer p, struct bt_msg *m)
{
    switch (m->id) {
    case BT_MKEEP_ALIVE:
        break;
    case BT_MCHOKE:
        p->peer_choking = 1;
        break;
    case BT_MINTERESTED:
        p->peer_interested = 1;
        break;
    case BT_MUNCHOKE:
        puts("got unchocked");
        p->peer_choking = 0;
        break;
    case BT_MNOT_INTERESTED:
        p->peer_interested = 0;
        break;
    case BT_MBITFIELD: {
        struct bt_msg_bitfield *bfield = (void *)m;
        if (bfield->len - 1 != CEIL_DIV(t->npieces, 8)) {
            puts("bitfield not matching number of pieces");
            return -1;
        }
        bt_bitset_read_arr(p->pieces, bfield->bitfield,
                           bfield->len - 1);
        fill_queue(p->requestq, p->pieces, t->piecetab, t->npieces);
    } break;
    case BT_MHAVE: {
        struct bt_msg_have *have = (void *)m;
        size_t i = have->piecei;
        int freq = ++t->piecetab[i].freq;
        bt_bitset_set(p->pieces, i);
        if (!t->piecetab[i].have)
            assert(bt_pqueue_insert(p->requestq, i, freq) != -1);
    } break;
    case BT_MREQUEST:
        break;
    case BT_MPIECE: {
        struct bt_msg_piece *mpiece = (void *)m;
        u32 pi = mpiece->piecei;
        u32 ci = mpiece->begin / CHUNK_SZ;
        if (mpiece->len - 9 != CHUNK_SZ) {
            puts("chunk size not matching request size");
        }

        if (bt_piece_add_chunk(&t->piecetab[pi], ci, mpiece->block) <
            0) {
            puts("error allocating piece");
            return -1;
        }
        if (t->piecetab[pi].have) {
            if (bt_piece_check(&t->piecetab[pi])) {
                printf("got piece %u\n", pi);
                bt_pqueue_remove(p->requestq);
                t->piecetab[pi].freq++;
                if (bt_piece_save(&t->piecetab[pi], t->mgr) < 0) {
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

            for (;;) {
                unsigned piecei = pq_min(p->requestq);
                BT_Piece piece = &t->piecetab[piecei];
                if (piece->have)
                    continue;
                size_t off = bt_piece_empty_chunk(piece) * CHUNK_SZ;
                size_t len = MIN(off + CHUNK_SZ, piece->length) - off;

                if (off >= piece->length) {
                    bt_pqueue_remove(p->requestq);
                    continue;
                } else {
                    printf("requesting piece: %u\n", piecei);
                    bt_msg_send(p->sockfd, BT_MREQUEST, piecei,
                                (unsigned)off, (unsigned)len);
                    break;
                }
            };
        }
    } else if (p->am_interested) {
        p->am_interested = 0;
        bt_msg_send(p->sockfd, BT_MNOT_INTERESTED);
    }
    return 0;
}

local int
change_state(u8 **pbuf, int state, size_t *rem)
{
    assert(pbuf && rem);
    u8 *buf = *pbuf;
    switch (state) {
    case ST_MSG_INIT:
        *rem = 4;
        *pbuf = bt_malloc(4);
        return ST_MSG_HEADER;
    case ST_MSG_HEADER: {
        u32 len;
        GET_U32BE(buf, len);
        if (!len)
            return ST_MSG_INIT;
        *pbuf = realloc(*pbuf, len + 4);
        if (!*pbuf) {
            perror("realloc()");
            return 0;
        }
        *rem = len;
        return ST_MSG_BODY;
    }
    case ST_MSG_BODY: {
        *rem = 0;
        return ST_MSG_INIT;
    }
    default:
        assert(0);
    }
    return ST_MSG_INIT;
}

local struct bt_msg *
read_loop(struct thread_data *thd, int sockfd)
{
    for (;;) {
        if (!thd->rem_rd) {
            size_t i = thd->rd_next - thd->mbuf;
            thd->rd_state = change_state(&thd->mbuf, thd->rd_state,
                                         &thd->rem_rd);
            if (thd->rd_state == ST_MSG_INIT) {
                struct bt_msg *m = bt_msg_unpack(thd->mbuf);
                free(thd->mbuf);
                thd->rd_next = thd->mbuf = NULL;
                return m;
            }
            thd->rd_next = thd->mbuf + i;
        }
        ssize_t xfer = read(sockfd, thd->rd_next, thd->rem_rd);
        if (xfer <= 0) {
            if (xfer < 0) {
                switch (errno) {
                case EINTR:
                    continue;
                case EWOULDBLOCK:
                    return NULL;
                default:
                    break;
                }
            }
            break;
        }

        thd->rem_rd -= xfer;
        thd->rd_next += xfer;
    }

    return NULL;
}

local int
set_nonblocking(int fd, int blocking)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1)
        return 0;

    if (blocking)
        flags &= ~O_NONBLOCK;
    else
        flags |= O_NONBLOCK;

    return fcntl(fd, F_SETFL, flags) != -1;
}

local void *
thread_func(struct thread_data *tdata)
{
    assert(tdata);
    unsigned id = tdata->id;
    BT_Torrent t = tdata->t;
    BT_Peer p = peer_new(t->addrtab[id]);
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
#define MAX_EVENTS 10

    struct bt_msg *msg;
    set_nonblocking(p->sockfd, 0);

    int epollfd;
    struct epoll_event ev, events[MAX_EVENTS];

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = tdata->mq_in;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, tdata->mq_in, &ev) == -1) {
        perror("epoll_ctl()");
        goto cleanup;
    }

    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = p->sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, p->sockfd, &ev) == -1) {
        perror("epoll_ctl()");
        goto cleanup;
    }

    bt_tqueue_insert(tdata->tq,
                     BT_TIMER(KEEPALIVE_IVAL, &tdata->mq_out[id][1],
                              keepalive));

    tdata->mbuf = NULL;
    tdata->rem_rd = 0;
    tdata->rd_state = ST_MSG_INIT;
    tdata->rd_next = tdata->mbuf;

#define RD_READY(pfd) ((pfd).revents & POLLIN)
#define WR_READY(pfd) ((pfd).revents & POLLIN)

    for (;;) {
        int nev = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nev == -1) {
            perror("epoll_wait()");
            exit(EXIT_FAILURE);
        }

        for (int i = 0; i < nev; i++) {
            if (events[i].data.fd == tdata->mq_in) {
                continue;
            } else if (events[i].data.fd == p->sockfd) {
                if (events[i].events & EPOLLIN) {
                    struct bt_msg *msg;
                    while ((msg = read_loop(tdata, p->sockfd))) {
                        set_nonblocking(p->sockfd, 1);
                        control_machine(t, p, msg);
                        set_nonblocking(p->sockfd, 0);
                        bt_msg_free(msg);
                    }
                }
            }
        }
    }
    perror("poll()");

cleanup:
    close(tdata->mq_in);
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
    int pipes[nthreads][2];
    pthread_t tidtab[nthreads + 1];

    BT_TQueue tq = bt_tqueue_new(2 * nthreads);
    if (!tq) {
        return -1;
    }

    for (unsigned i = 0; i < nthreads; i++) {
        if (pipe2(pipes[i], O_DIRECT) < 0) {
            perror("pipe2()");
            return -1;
        }
        tdatatab[i].id = i;
        tdatatab[i].nclients = nthreads;
        tdatatab[i].t = t;
        tdatatab[i].tq = tq;
        tdatatab[i].mq_in = pipes[i][0];
        tdatatab[i].mq_out = pipes;
    }

    pthread_create(&tidtab[nthreads + 1], NULL,
                   (void *(*)(void *))bt_timerthread, tq);

    for (unsigned i = 0; i < nthreads; i++) {
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
