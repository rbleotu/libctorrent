#ifndef THREAD_MSG_H_
#define THREAD_MSG_H_

#include "util/common.h"

enum {
    BT_THMSG_KEEPALIVE,
    BT_THMSG_HAVE,
    BT_THMSG_CHOKE,
    BT_THMSG_UNCHOKE,

    BT_THMSG_CNT
};

struct bt_thmsg {
    uint8 id;
    uint32 peer_id;
};

local const struct bt_thmsg KEEPALIVE = {BT_THMSG_KEEPALIVE, 0};
local const struct bt_thmsg CHOKE = {BT_THMSG_CHOKE, 0};
local const struct bt_thmsg UNCHOKE = {BT_THMSG_UNCHOKE, 0};

int
bt_thmsg_send(int pfd, struct bt_thmsg m);

int
bt_thmsg_recv(int pfd, struct bt_thmsg *m);

#endif
