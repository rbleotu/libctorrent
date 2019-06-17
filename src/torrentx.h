#ifndef TORRENTX_H_
#define TORRNETX_H_

#include "disk.h"
#include "crypt/sha1.h"
#include "piece.h"
#include "torrent.h"
#include "util/common.h"
#include "peer.h"
#include "eventqueue.h"
#include "timer_queue.h"

#define MAX_UNCHOKED 10
#define UNCHOKE_DT   20

struct bt_torrent {
    struct bt_settings settings;

    uint8 info_hash[SHA1_DIGEST_LEN];

    const char *announce[MAX_TRACKERS + 1];

    BT_DiskMgr mgr;
    off_t size, size_have;

    unsigned naddr;
    struct bt_peer_addr *addrtab;

    BT_Peer unchoked[MAX_UNCHOKED];
    time_t last_chokealgo;

    size_t piece_length;
    unsigned npieces, nhave;

    BT_Piece want[16];
    int nwant;

    struct bt_eventqueue evqueue;
    struct bt_timerqueue tmqueue;

    struct bt_piece piecetab[];
};

#define logx(t, x, ...) ((t)->settings.logger.log ? (t)->settings.logger.log(x, __VA_ARGS__) : (void) 0)
#define log_info(t, ...) logx(t,  BT_LOG_INFO, __VA_ARGS__)
#define log_warn(t, ...) logx(t,  BT_LOG_WARN, __VA_ARGS__)
#define log_error(t, ...) logx(t, BT_LOG_ERROR, __VA_ARGS__)

static inline BT_Piece
bt_torrent_get_piece(BT_Torrent t, uint32 i)
{
    return &t->piecetab[i];
}

static inline bool
bt_torrent_has_piece(BT_Torrent t, uint32 i)
{
    return bt_piece_complete(&t->piecetab[i]);
}

static inline void
bt_torrent_piece_completed(BT_Torrent t, BT_Piece piece)
{
    bt_eventqueue_push(&t->evqueue, BT_EVENT(BT_EVPIECE_COMPLETE, piece, NULL));
}

#endif
