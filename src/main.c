#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

#include "util/common.h"
#include "util/vector.h"
#include "torrentx.h"
#include "torrent.h"
#include "peer.h"

static void
print_peer_stats(BT_Torrent t, BT_Peer peer)
{
    byte ip[4];
    PUT_U32BE(ip, peer->ipv4);
    printf("\033[34;1m%3hhu.%3hhu.%3hhu.%3hhu\033[0m\t\t", ip[0], ip[1], ip[2], ip[3]);
    printf("%-8.2lf\t\t", 100. * bt_peer_progress(t, peer));
    printf("%-12zu\t\t", peer->pieceshas.n);
    printf("\033[31;1m%-2.2lf\033[0m\t\t", bt_peer_dlrate(peer) / 1024.);
    printf("\033[34;1m%8.2lf\033[0m\t\t", bt_peer_ulrate(peer) / 1024.);
    printf("%-8zu\t", peer->nrequests);
    printf("%2d%d%d%d", peer->am_interested, peer->am_choking, peer->peer_interested, peer->peer_choking);
    putc('\n', stdout);
}

static inline void ansi_cursor_up(int n) { printf("\033[%dA", n);}

static int
piece_received(BT_Torrent t, BT_Peer peer)
{
    static time_t tlast;
    time_t now = time(NULL);
    if (now - tlast < 1)
        return 0;

    printf("\033[2J");
    printf("\033[H");
    printf("peer (ipv4)\t\tcompleted (%)\t\tnpieces\t\tulrate (KiB/s)\t\tdlrate (KiB/s)\t\tenqued req\tstate\n");
    int nlines = 0;

    VECTOR_FOREACH(&t->peers, i, struct bt_peer) {
        if (i->connected) {
            print_peer_stats(t, i);
            nlines++;
        }
    }

    puts("-------------------------------------");
    //ansi_cursor_up(nlines + 2);

    tlast = now;
    return 0;
}

static char *
sprinttime(char dest[], size_t n)
{
    time_t t = time(NULL);

    struct tm *timebuf = localtime(&t);
    if (!timebuf)
        return "??:??:??";

    strftime(dest, n, "%T", timebuf);
    return dest;
}

static void
cli_logger(int type, const char *fmt, ...)
{
    char strtime[32];
    size_t len = strlen(fmt);
    va_list ap;

    switch (type) {
    case BT_LOG_WARN:
        fprintf(stderr, "[\033[1;33mWARNING\033[0m %8s] ", sprinttime(strtime, sizeof(strtime)));
        break;
    case BT_LOG_ERROR:
        fprintf(stderr, "[\033[1;31m ERROR \033[0m %8s] ", sprinttime(strtime, sizeof(strtime)));
        break;
    case BT_LOG_INFO:
    default:
        fprintf(stderr, "[\033[1;34m INFO  \033[0m %8s] ", sprinttime(strtime, sizeof(strtime)));
        break;
    }

    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);

    if (len && fmt[len-1] == ':')
        fprintf(stderr, " %s", bt_strerror());
    putc('\n', stderr);
}

static const struct bt_settings DEFAULT_SETTINGS = {
    .logger        = {cli_logger},
    .metainfo_path = NULL,
    .port          = 1221,
    .outdir        = ".",
    .max_peers     = 32,
};

int
main(int argc, char *argv[])
{
    struct bt_settings settings = DEFAULT_SETTINGS;
    if (argv[1]) {
        settings.metainfo_path = argv[1];
    }

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

    if ((peer = bt_torrent_tracker_request(t, 50)) == 0) {
        fputs("No tracker could be contacted\n", stderr);
        return 1;
    }

    printf("got %u peers\n", peer);

    bt_torrent_add_action(t, BT_EVPEER_PIECE, piece_received);

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
