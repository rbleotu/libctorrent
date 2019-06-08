#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "util/error.h"
#include "tracker.h"
#include "tracker_udp.h"

enum {
    PROTO_UDP,
    PROTO_HTTP,

    PROTO_CNT
};

local const char *proto_name[] = {
        [PROTO_UDP] = "udp://",
        [PROTO_HTTP] = "http://",
};

local int
get_proto(const char *url)
{
    int i = 0;
    for (; i < PROTO_CNT; i++)
        if (!strncmp(proto_name[i], url, strlen(proto_name[i])))
            return i;
    return i;
}

local inline int
get_domain_port(char domain[], size_t len, uint16_t *port,
                const char *url)
{

    char fmt[64];
    snprintf(fmt, sizeof(fmt), "%%*[^:]://%%%zu[^:]:%%hu", len);
    if (sscanf(url, fmt, domain, port) != 2)
        return -1;
    return 0;
}

#define DOMAIN_LEN 64
local void
generate_peer_id(uint8_t dest[static 20])
{
    dest[0] = 'B', dest[1] = 'T';
    srand(time(NULL));
    for (int i = 2; i < 20; i++) {
        int r = ((unsigned)rand()) % 36;
        dest[i] = r < 26 ? 'a' + r : '0' + r;
    }
}

unsigned
bt_tracker_request(OUT struct tracker_response *resp,
                   IN const char *url, IN struct tracker_request *req)
{
    char domain[DOMAIN_LEN];
    uint16_t port;
    bt_errno = 0;

    if (get_domain_port(domain, sizeof(domain), &port, url) < 0) {
        bt_errno = BT_ETRACKERURL;
        return 0;
    }

    generate_peer_id(req->peer_id);

    switch (get_proto(url)) {
    case PROTO_UDP:
        return bt_tracker_udp_request(resp, domain, port, req);
        break;
    case PROTO_HTTP:
        /* TODO */
        return 0;
        break;
    default:
        bt_errno = BT_EBADPROTO;
        return 0;
    }

    return 0;
}
