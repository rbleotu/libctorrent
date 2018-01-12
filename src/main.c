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

int
main(int argc, char *argv[])
{
    FILE *f = freopen(argv[1], "rb", stdin);
    if (!f) {
        fprintf(stderr, "Failed to open '%s': %s\n", argv[1],
                strerror(errno));
        return 1;
    }

    BT_Torrent t = bt_torrent_new(f, "./out/", 1222);
    if (!t) {
        fprintf(stderr, "Error reading torrent '%s': %s\n", argv[1],
                bt_strerror());
        return 1;
    }

    if (bt_torrent_check(t) < 0) {
        fprintf(stderr, "Torrent check failed: %s\n", bt_strerror());
        return 1;
    }

    fclose(f);
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
