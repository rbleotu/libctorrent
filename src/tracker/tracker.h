#ifndef TRACKER_H_
#define TRACKER_H_

#include "../common.h"

struct tracker_request {
    u8 peer_id[20];
    u8 info_hash[20];
    u64 downloaded;
    u64 left;
    u64 uploaded;
    u32 transaction_id;
    u32 num_want;
    u32 event;
    u16 port;
};

struct tracker_response {
    u32 leechers, seeders;
    struct bt_peer_addr *peertab;
};

unsigned
bt_tracker_request(OUT struct tracker_response *resp,
                   IN const char *url,
                   IN struct tracker_request *req);

#endif
