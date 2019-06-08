#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <assert.h>
#include <stdarg.h>
#include <time.h>

#include <torrent.h>

static void
piece_received(BT_Torrent t)
{
    puts("received a piece");
    bt_torrent_pause(t);
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
    char strtime[16];
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
    .port      = 1221,
    .outdir    = ".",
    .max_peers = 32,
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
