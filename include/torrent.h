#ifndef TORRENT_H_
#define TORRENT_H_

#include "util/error.h"
#include "util/logger.h"
#include "event.h"

#define MAX_TRACKERS 256
typedef struct bt_torrent *BT_Torrent;

enum {
    BT_STATUS_DONE,
    BT_STATUS_PAUSED,

    BT_STATUS_CNT
};


struct bt_settings {
    BT_Logger logger;

    const char *metainfo_path;

    unsigned port;

    const char *outdir;

    unsigned max_peers;
};

typedef struct bt_settings *BT_Settings;

extern BT_Torrent bt_torrent_new(BT_Settings settings);
extern int bt_torrent_start(BT_Torrent t);
extern int bt_torrent_check(BT_Torrent t);
extern int bt_torrent_tracker_request(BT_Torrent t, unsigned npeer);
extern int bt_torrent_pause(BT_Torrent t);
extern void bt_torrent_free(BT_Torrent t);
extern void bt_torrent_add_action(BT_Torrent t, int ev, int (*cb)());
extern unsigned bt_torrent_get_nhave(BT_Torrent t);
extern double bt_torrent_progress(BT_Torrent t);

#endif
