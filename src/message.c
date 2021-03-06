#include <unistd.h>
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <setjmp.h>
#include <stdarg.h>
#include <assert.h>

#include "util/common.h"
#include "util/error.h"
#include "bitset.h"
#include "message.h"

local struct bt_msg prepared[] = {
        [BT_MKEEP_ALIVE] =     {0, BT_MKEEP_ALIVE},
        [BT_MCHOKE] =          {1, BT_MCHOKE},
        [BT_MUNCHOKE] =        {1, BT_MUNCHOKE},
        [BT_MINTERESTED] =     {1, BT_MINTERESTED},
        [BT_MNOT_INTERESTED] = {1, BT_MNOT_INTERESTED}
};

struct bt_msg *
bt_msg_new(int id, ...)
{
    if (id < BT_MHAVE)
        return &prepared[id];
    if (id == BT_MKEEP_ALIVE)
        return &prepared[id];

    va_list ap;
    void *res = NULL;
    va_start(ap, id);
    switch (id) {
    case BT_MREQUEST: {
        struct bt_msg_request *m = bt_malloc(sizeof(*m));
        if (!m) {
            bt_errno = BT_EALLOC;
            break;

        }
        m->len = 13;
        m->id = BT_MREQUEST;
        m->piecei = va_arg(ap, uint32);
        m->begin = va_arg(ap, uint32);
        m->length = va_arg(ap, uint32);
        res = m;
      }
      break;
    case BT_MHAVE: {
        struct bt_msg_have *m = bt_malloc(sizeof(*m));
        if (!m) {
            bt_errno = BT_EALLOC;
            break;
        }
        m->len = 5;
        m->id = BT_MHAVE;
        m->piecei = va_arg(ap, uint32);
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
    if (m->id < BT_MHAVE || m->id == BT_MKEEP_ALIVE)
        return;
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

//int
//bt_handshake_send(const struct bt_msg_handshake *hshk, int sockfd)
//{
//    char buf[sizeof(*hshk)];
//    memcpy(buf, hshk, 20);
//    memcpy(buf + 20, &hshk->reserved, 8);
//    memcpy(buf + 28, &hshk->info_hash, 20);
//    memcpy(buf + 48, &hshk->peer_id, 20);
//    return writen(sockfd, buf, HSHK_LEN);
//}
//
//int
//bt_handshake_recv(struct bt_msg_handshake *hshk, int sockfd)
//{
//    char buf[sizeof(*hshk)];
//    if (readn(sockfd, buf, HSHK_LEN) < 0)
//        return -1;
//    memcpy(hshk, buf, 20);
//    memcpy(&hshk->reserved, buf + 20, 8);
//    memcpy(&hshk->info_hash, buf + 28, 20);
//    memcpy(&hshk->peer_id, buf + 48, 20);
//    return 0;
//}

#define safe_readn(fd, buf, len)                             \
    do {                                                     \
        if (readn((fd), (buf), (len)) != ((ssize_t)(len))) { \
            bt_errno = BT_ENETRECV;                          \
            goto cleanup;                                    \
        }                                                    \
    } while (0)

#define safe_writen(fd, buf, len)                             \
    do {                                                      \
        if (writen((fd), (buf), (len)) != ((ssize_t)(len))) { \
            bt_errno = BT_ENETRECV;                           \
            goto cleanup;                                     \
        }                                                     \
    } while (0)

int
bt_msg_send(int sockfd, int id, ...)
{
    char buf[128];
    va_list ap;
    va_start(ap, id);

    if (id < BT_MHAVE) {
        uint32 len = prepared[id].len;
        PUT_U32BE(buf, len);
        buf[4] = id;
        safe_writen(sockfd, buf, len + 4);
    } else {
        switch (id) {
        case BT_MKEEP_ALIVE:
            PUT_U32BE(buf, 0);
            safe_writen(sockfd, buf, 4);
            break;
        case BT_MREQUEST: {
            uint32 len = 13;
            uint32 piecei = va_arg(ap, uint32);
            uint32 begin = va_arg(ap, uint32);
            uint32 length = va_arg(ap, uint32);

            PUT_U32BE(buf, len);
            buf[4] = id;
            PUT_U32BE(buf + 5, piecei);
            PUT_U32BE(buf + 9, begin);
            PUT_U32BE(buf + 13, length);
            safe_writen(sockfd, buf, len + 4);
            break;
        }
        default:
            assert(0);
        }
    }

    va_end(ap);
    return 0;
cleanup:
    va_end(ap);
    return -1;
}

struct bt_msg *
bt_msg_unpack(const uint8 buf[])
{
    uint32 len, id;
    GET_U32BE(buf, len);
    if (!len)
        return &prepared[BT_MKEEP_ALIVE];
    id = buf[4];
    if (id < BT_MHAVE)
        return &prepared[id];

    struct bt_msg *msg = bt_malloc(len + 4);
    if (!msg) {
        bt_errno = BT_EALLOC;
        return NULL;
    }

    msg->len = len;
    msg->id = id;

    assert (len < (17<<10));

    switch (id) {
    case BT_MHAVE: {
        struct bt_msg_have *have = (void *)msg;
        GET_U32BE(buf + 5, have->piecei);
        break;
    }
    case BT_MBITFIELD: {
        struct bt_msg_bitfield *bfield = (void *)msg;
        memcpy(bfield->bitfield, buf + 5, len - 1);
    } break;
    case BT_MREQUEST: {
        struct bt_msg_request *req = (void *)msg;
        GET_U32BE(buf + BT_MSG_LEN, req->piecei);
        GET_U32BE(buf + BT_MSG_LEN + 4, req->begin);
        GET_U32BE(buf + BT_MSG_LEN + 8, req->length);
    } break;
    case BT_MPIECE: {
        struct bt_msg_piece *piece = (void *)msg;
        GET_U32BE(buf + BT_MSG_LEN, piece->piecei);
        GET_U32BE(buf + BT_MSG_LEN + 4, piece->begin);
        memcpy(piece->block, buf, len - 9);
    } break;
    }

    return msg;
}

struct bt_msg *
bt_msg_recv(int sockfd)
{
    uint8 buf[128];
    uint32 len;
    uint8 id;

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

struct bt_handshake *
bt_handshake_unpack(const byte src[])
{
    struct bt_handshake *hsk = malloc(sizeof(*hsk));
    if (!hsk) {
        perror("malloc");
        return NULL;

    }
    size_t i = 0;
    hsk->id = BT_MSG_HANDSHAKE;
    hsk->len = HSHK_LEN - 4;

    memcpy(hsk->magic, src+i, sizeof(hsk->magic)), i += sizeof(hsk->magic);
    memcpy(hsk->reserved, src+i, sizeof(hsk->reserved)), i+= sizeof(hsk->reserved);
    memcpy(hsk->info_hash, src+i, SHA1_DIGEST_LEN), i += SHA1_DIGEST_LEN;
    memcpy(hsk->peer_id, src+i, 20), i += 20;

    return hsk;
}

void
bt_msg_pack(byte dest[], struct bt_msg *msg)
{
    uint32 i = 0;
    if (msg->id < BT_MSG_CNT) {
        PUT_U32BE(dest + i, msg->len), i += 4;
        dest[i] = msg->id, i++;
    }

    if (msg->id == BT_MKEEP_ALIVE) {
        PUT_U32BE(dest + i, 0), i += 4;
        return;
    }

    if (msg->id < BT_MHAVE)
        return;

    switch (msg->id) {
    case BT_MHAVE:
        {
            struct bt_msg_have *have = (void *)msg;
            PUT_U32BE(dest + i, have->piecei), i += 4;
            break;
        }
    case BT_MSG_HANDSHAKE:
        {
            struct bt_handshake *hsk = (void *)msg;
            memcpy(dest+i, hsk->magic, sizeof(hsk->magic)), i += sizeof(hsk->magic);
            memset(dest+i, 0, sizeof(hsk->reserved)), i+= sizeof(hsk->reserved);
            memcpy(dest+i, hsk->info_hash, SHA1_DIGEST_LEN), i += SHA1_DIGEST_LEN;
            memcpy(dest+i, hsk->peer_id, 20), i += 20;
            break;
        }
    case BT_MREQUEST:
        {
            struct bt_msg_request *req = (void *)msg;
            PUT_U32BE(dest + i, req->piecei), i += 4;
            PUT_U32BE(dest + i, req->begin), i += 4;
            PUT_U32BE(dest + i, req->length), i += 4;
        }
        break;
    default:
        assert (!"not implmented");
    }
}
