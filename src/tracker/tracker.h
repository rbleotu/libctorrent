#ifndef TRACKER_H_
#define TRACKER_H_

#include "../common.h"

struct peer {
    uint32_t ipv4;
    uint16_t port;
};

struct tracker_request {
    uint8_t peer_id[20];
    uint8_t info_hash[20];
    uint64_t downloaded;
    uint64_t left;
    uint64_t uploaded;
    uint32_t transaction_id;
    uint32_t num_want;
    uint32_t event;
    uint16_t port;
};

struct tracker_response {
    uint32_t leechers, seeders;
    struct peer *peertab;
};

unsigned
bt_tracker_request(OUT struct tracker_response *resp,
                   IN const char *url,
                   IN struct tracker_request *req);

#endif
