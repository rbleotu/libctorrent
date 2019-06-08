#ifndef PEER_H_
#define PEER_H_

#include "util/common.h"
#include "bitset.h"
#include "pqueue.h"
typedef struct bt_peer *BT_Peer;

struct bt_peer {
    int sockfd;

    int am_choking;
    int am_interested;

    int peer_choking;
    int peer_interested;

    BT_PQueue requestq;

    BT_Bitset pieces;
};

#endif
