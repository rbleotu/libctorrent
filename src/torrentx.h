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

#define logx(t, x, ...) ((t)->settings.logger.log ? (t)->settings.logger.log(x, __VA_ARGS__) : (void) 0)
#define log_info(t, ...) logx(t,  BT_LOG_INFO, __VA_ARGS__)
#define log_warn(t, ...) logx(t,  BT_LOG_WARN, __VA_ARGS__)
#define log_error(t, ...) logx(t, BT_LOG_ERROR, __VA_ARGS__)

#endif
