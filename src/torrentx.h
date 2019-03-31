#ifndef TORRENTX_H_
#define TORRNETX_H_

#include "disk/disk.h"
#include "crypt/sha1.h"
#include "piece.h"
#include "torrent.h"
#include "common.h"

struct bt_torrent {
    struct bt_settings settings;

    u8 info_hash[SHA1_DIGEST_LEN];

    const char *announce[MAX_TRACKERS + 1];

    BT_DiskMgr mgr;
    off_t size, size_have;

    unsigned naddr;
    struct bt_peer_addr *addrtab;

    size_t piece_length;
    unsigned npieces, nhave;

    struct bt_piece piecetab[];
};

#endif
