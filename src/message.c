#include <unistd.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <assert.h>

#include "common.h"
#include "error.h"
#include "bitset.h"
#include "message.h"

local struct bt_msg prepared[] = {
        [BT_MKEEP_ALIVE] = {0, BT_MKEEP_ALIVE},
        [BT_MCHOKE] = {1, BT_MKEEP_ALIVE},
        [BT_MUNCHOKE] = {1, BT_MKEEP_ALIVE},
        [BT_MINTERESTED] = {1, BT_MKEEP_ALIVE},
        [BT_MNOT_INTERESTED] = {1, BT_MKEEP_ALIVE}};


struct bt_msg *
bt_msg_new(int id, ...)
{
    if (id < BT_MHAVE)
        return &prepared[id];
    va_list ap;
    void *res = NULL;
    va_start(ap, id);
    switch (id) {
    case BT_MHAVE: {
        struct bt_msg_have *m = bt_malloc(sizeof(*m));
        if (!m) {
            bt_errno = BT_EALLOC;
            break;
        }
        m->len = 5;
        m->id = BT_MHAVE;
        m->piecei = va_arg(ap, u32);
        res = m;
    } break;
    default:
        assert(0);
    }
    va_end(ap);
    return res;
}

void
bt_msg_free(struct bt_msg *m)
{
    if (m->id < BT_MHAVE)
        return;
    if (m->id == BT_MPIECE) {
        struct bt_msg_piece *pm = (void *)m;
        free(pm->block);
    }
    free(m);
}

local ssize_t
writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0; /* and call write() again */
            else
                return (-1); /* error */
        }

        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

local ssize_t
readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            break; /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

#define HSHK_LEN 68

int
bt_handshake_send(const struct bt_msg_handshake *hshk, int sockfd)
{
    char buf[sizeof(*hshk)];
    memcpy(buf, hshk, 20);
    memcpy(buf + 20, &hshk->reserved, 8);
    memcpy(buf + 28, &hshk->info_hash, 20);
    memcpy(buf + 48, &hshk->peer_id, 20);
    return writen(sockfd, buf, HSHK_LEN);
}

int
bt_handshake_recv(struct bt_msg_handshake *hshk, int sockfd)
{
    char buf[sizeof(*hshk)];
    if (readn(sockfd, buf, HSHK_LEN) < 0)
        return -1;
    memcpy(hshk, buf, 20);
    memcpy(&hshk->reserved, buf + 20, 8);
    memcpy(&hshk->info_hash, buf + 28, 20);
    memcpy(&hshk->peer_id, buf + 48, 20);
    return 0;
}

#define safe_readn(fd, buf, len)                             \
    do {                                                     \
        if (readn((fd), (buf), (len)) != ((ssize_t)(len))) { \
            bt_errno = BT_ENETRECV;                          \
            goto cleanup;                                    \
        }                                                    \
    } while (0)

struct bt_msg *
bt_msg_recv(int sockfd)
{
    u8 buf[128];
    u32 len;
    u8 id;

    if (readn(sockfd, &len, 4) != 4)
        return NULL;
    len = ntohl(len);
    if (len == 0)
        return &prepared[BT_MKEEP_ALIVE];
    if (readn(sockfd, &id, 1) != 1)
        return NULL;
    if (id < BT_MHAVE)
        return &prepared[id];

    struct bt_msg *msg = bt_malloc(len + 4);
    if (!msg) {
        bt_errno = BT_EALLOC;
        return NULL;
    }

    msg->len = len;
    msg->id = id;

    switch (id) {
    case BT_MHAVE:
        safe_readn(sockfd, buf + BT_MSG_LEN, len - 1);
        {
            struct bt_msg_have *have = (void *)msg;
            GET_U32BE(buf + BT_MSG_LEN, have->piecei);
        }
        break;
    case BT_MBITFIELD: {
        struct bt_msg_bitfield *bfield = (void *)msg;
        safe_readn(sockfd, bfield->bitfield, len - 1);
    } break;
    case BT_MREQUEST:
        safe_readn(sockfd, buf + BT_MSG_LEN, len - 1);
        {
            struct bt_msg_request *req = (void *)msg;
            GET_U32BE(buf + BT_MSG_LEN, req->piecei);
            GET_U32BE(buf + BT_MSG_LEN + 4, req->begin);
            GET_U32BE(buf + BT_MSG_LEN + 8, req->length);
        }
        break;
    case BT_MPIECE:
        safe_readn(sockfd, buf + BT_MSG_LEN, 8);
        {
            struct bt_msg_piece *piece = (void *)msg;
            GET_U32BE(buf + BT_MSG_LEN, piece->piecei);
            GET_U32BE(buf + BT_MSG_LEN + 4, piece->begin);
            safe_readn(sockfd, piece->block, len - 9);
        }
        break;
    }

    return msg;
cleanup:
    free(msg);
    return NULL;
}
