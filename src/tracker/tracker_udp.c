#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include "../common.h"
#include "../error.h"
#include "tracker.h"

#define CONREQ_LEN 16
#define CONRESP_LEN 16
#define ANNREQ_LEN 98
#define ANNRESP_LEN 20

#define DEF_TIMEOUT 2

struct connect_request {
    uint64_t protocol_id;
    uint32_t action;
    uint32_t transaction_id;
};

struct connect_response {
    uint32_t action;
    uint32_t transaction_id;
    uint64_t connection_id;
};

struct announce_request {
    uint64_t connection_id;
    uint32_t action;
    uint32_t transaction_id;
    uint8_t info_hash[20];
    uint8_t peer_id[20];
    uint64_t downloaded;
    uint64_t left;
    uint64_t uploaded;
    uint32_t event;
    uint32_t IP;
    uint32_t key;
    uint32_t num_want;
    uint16_t port;
};

typedef struct peer_info PeerInfo;

struct peer_info {
    uint32_t ipv4;
    uint16_t tcp_port;
};

struct announce_response {
    uint32_t action;
    uint32_t transaction_id;
    uint32_t interval;
    uint32_t leechers;
    uint32_t seeders;
    struct peer *peers;
};

static inline void
pack_connect(uint8_t buf[], struct connect_request req)
{
    PUT_U64BE(buf, req.protocol_id);
    PUT_U32BE(buf + 8, req.action);
    PUT_U32BE(buf + 12, req.transaction_id);
}

static inline void
unpack_connect(const uint8_t buf[], struct connect_response *resp)
{
    GET_U32BE(buf, resp->action);
    GET_U32BE(buf + 4, resp->transaction_id);
    GET_U64BE(buf + 8, resp->connection_id);
}

static inline void
pack_announce(uint8_t buf[], struct announce_request req)
{
    PUT_U64BE(buf, req.connection_id);
    PUT_U32BE(buf + 8, req.action);
    PUT_U32BE(buf + 12, req.transaction_id);
    memcpy(buf + 16, req.info_hash, 20);
    memcpy(buf + 36, req.peer_id, 20);
    PUT_U64BE(buf + 56, req.downloaded);
    PUT_U64BE(buf + 64, req.left);
    PUT_U64BE(buf + 72, req.uploaded);
    PUT_U32BE(buf + 80, req.event);
    PUT_U32BE(buf + 84, req.IP);
    PUT_U32BE(buf + 88, req.key);
    PUT_U32BE(buf + 92, req.num_want);
    PUT_U16BE(buf + 96, req.port);
}

static inline void
unpack_announce(const uint8_t buf[], struct announce_response *resp)
{
    GET_U32BE(buf, resp->action);
    GET_U32BE(buf + 4, resp->transaction_id);
    GET_U32BE(buf + 8, resp->interval);
    GET_U32BE(buf + 12, resp->leechers);
    GET_U32BE(buf + 16, resp->seeders);
}

local void
timeout_handler(int sig)
{
    ;
}

static int
tracker_connect(OUT struct connect_response *conresp,
                IN struct connect_request *conreq, int sockfd,
                unsigned timeout)
{
    u8 buf[256];
    ssize_t xfer;

    pack_connect(buf, *conreq);

    if (sendto(sockfd, buf, CONREQ_LEN, 0, NULL, 0) < 0) {
        bt_errno = BT_EUDPSEND;
        return -1;
    }


    struct sigaction sa;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sa.sa_handler = timeout_handler;
    if (sigaction(SIGALRM, &sa, NULL) == -1) {
        bt_errno = BT_ESIGACTION;
        return -1;
    }

    alarm(timeout);

    if ((xfer = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL)) <
        0) {
        if (errno = EINTR)
            bt_errno = BT_ETIMEOUT;
        return -1;
    }
    alarm(0);

    if (xfer < 16) {
        bt_errno = BT_EBADRESPONSE;
        return -1;
    }

    unpack_connect(buf, conresp);
    if (conresp->transaction_id != conreq->transaction_id)
        return -1;
    if (conresp->action != 0)
        return -1;

    return 0;
}

static inline int
unpack_peers(u8 buf[], size_t len, struct announce_response *resp)
{
    struct peer *v = resp->peers;
    int res = (len - 20) / 6;
    while (len > 20) {
        GET_U32BE(buf, v->ipv4);
        GET_U16BE(buf + 4, v->port);
        buf += 6, len -= 6, ++v;
    }
    return res;
}

static int
tracker_announce(OUT struct announce_response *annresp,
                 IN struct announce_request *annreq, int sockfd,
                 unsigned timeout)
{
    u8 buf[256];
    ssize_t xfer;
    pack_announce(buf, *annreq);
    if (sendto(sockfd, buf, ANNREQ_LEN, 0, NULL, 0) < 0)
        return -1;
    if ((xfer = recvfrom(sockfd, buf, sizeof(buf), 0, NULL, NULL)) <
        0) {
        if (errno == EINTR) {
            return 0;
        }
        return -1;
    }
    if (xfer < 20)
        return -1;
    unpack_announce(buf, annresp);
    if (annresp->transaction_id != annreq->transaction_id)
        return -1;
    if (annresp->action != 1)
        return -1;
    return unpack_peers(buf + 20, xfer, annresp);
}

local unsigned
udp_request(OUT struct tracker_response *resp, int sockfd,
            IN struct tracker_request *req)
{
    struct connect_request conreq;
    struct connect_response conresp;
    struct announce_request annreq;
    struct announce_response annresp;

    /* Connect request */
    conreq = (struct connect_request){.protocol_id = 0x41727101980ULL,
                                      .action = 0,
                                      .transaction_id =
                                              req->transaction_id};
    if (tracker_connect(&conresp, &conreq, sockfd, DEF_TIMEOUT) < 0)
        return 0;

    /* Announce request */
    annreq = (struct announce_request){
            .connection_id = conresp.connection_id,
            .action = 1,
            .transaction_id = req->transaction_id,
            .downloaded = req->downloaded,
            .left = req->left,
            .uploaded = req->uploaded,
            .event = req->event,
            .IP = 0,
            .key = 0,
            .num_want = req->num_want,
            .port = req->port};
    memcpy(annreq.info_hash, req->info_hash, 20);
    memcpy(annreq.peer_id, req->peer_id, 20);
    annresp.peers = resp->peertab;
    int res =
            tracker_announce(&annresp, &annreq, sockfd, DEF_TIMEOUT);
    close(sockfd);
    return res < 0 ? 0 : res;
}

unsigned
bt_tracker_udp_request(OUT struct tracker_response *resp,
                       IN char domain[], IN uint16_t port,
                       IN struct tracker_request *req)
{
    int sockfd;
    char port_str[8];
    snprintf(port_str, sizeof(port_str), "%hu", port);

    struct addrinfo hints = {.ai_family = AF_INET,
                             .ai_socktype = SOCK_DGRAM};
    struct addrinfo *servinfo, *p;

    if (getaddrinfo(domain, port_str, &hints, &servinfo) != 0) {
        bt_errno = BT_EGETADDR;
        return 0;
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            continue;
        }
        break;
    }

    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd < 0)
        return 0;

    if (connect(sockfd, p->ai_addr, p->ai_addrlen) < 0) {
        fputs("Failed to connect\n", stderr);
        return 0;
    }

    unsigned n = udp_request(resp, sockfd, req);

    close(sockfd);
    return n;
}
