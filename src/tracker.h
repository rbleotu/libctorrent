#ifndef TRACKER_H_
#define TRACKER_H_

#include "util/common.h"

struct tracker_request {
    uint8 peer_id[20];
    uint8 info_hash[20];
    uint64 downloaded;
    uint64 left;
    uint64 uploaded;
    uint32 transaction_id;
    uint32 num_want;
    uint32 event;
    uint16 port;
};

struct tracker_response {
    uint32 leechers, seeders;
    struct bt_peer_addr *peertab;
};

unsigned
bt_tracker_request(OUT struct tracker_response *resp,
                   IN const char *url,
                   IN struct tracker_request *req);

#endif
