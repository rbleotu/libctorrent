#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <assert.h>

#include <torrent.h>

static void
piece_received(BT_Torrent t)
{
    puts("received a piece");
    bt_torrent_pause(t);
}

static const struct bt_settings DEFAULT_SETTINGS = {
    .logger        = {NULL},
    .metainfo_path = NULL,
    .port      = 1221,
    .outdir    = ".",
    .max_peers = 32,
};

int
main(int argc, char *argv[])
{
    struct bt_settings settings = DEFAULT_SETTINGS;
    if (argv[1])
        settings.metainfo_path = argv[1];

    BT_Torrent t = bt_torrent_new(&settings);
    if (!t) {
        fprintf(stderr, "Error reading torrent '%s': %s\n", argv[1],
                bt_strerror());
        return 1;
    }

    if (bt_torrent_check(t) < 0) {
        fprintf(stderr, "Torrent check failed: %s\n", bt_strerror());
        return 1;
    }

    printf("have:%u\n", bt_torrent_get_nhave(t));

    unsigned peer;

    if ((peer = bt_torrent_tracker_request(t, 20)) == 0) {
        fputs("No tracker could be contacted\n", stderr);
        return 1;
    }

    printf("got %u peers\n", peer);


    // bt_torrent_add_action(t, BT_EVENT_GOTPIECE, NULL);

    int status;

    while ((status = bt_torrent_start(t)) > 0) {
        switch (status) {
        case BT_STATUS_PAUSED:
            break;
        default:
            assert("Unknown status code" && 0);
        }
    }
    if (status < 0) {
        fprintf(stderr, "Error downloading data: %s\n",
                bt_strerror());
        return 1;
    }

    return 0;
}
