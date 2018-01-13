#ifndef TORRENT_H_
#define TORRENT_H_

#include <error.h>

#define MAX_TRACKERS 256
typedef struct bt_torrent *BT_Torrent;

enum {
    BT_STATUS_DONE,
    BT_STATUS_PAUSED,

    BT_STATUS_CNT
};

enum {
    BT_EVENT_NONE,
    BT_EVENT_GOTPIECE,

    BT_EVENT_CNT
};

extern BT_Torrent
bt_torrent_new(FILE *fin, const char *outdir, unsigned short port);

extern int
bt_torrent_start(BT_Torrent t);

extern int
bt_torrent_check(BT_Torrent t);

extern int
bt_torrent_tracker_request(BT_Torrent t, unsigned npeer);

extern int
bt_torrent_pause(BT_Torrent t);

extern void
bt_torrent_free(BT_Torrent t);

extern void
bt_torrent_add_action(BT_Torrent t, int ev, void *h);

extern unsigned
bt_torrent_get_nhave(BT_Torrent t);
#endif
