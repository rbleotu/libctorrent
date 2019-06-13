#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "peer.h"
#include "net.h"
#include "../../include/torrent.h"

#define LOCALHOST ((127<<24) | 1)
#define PORT      8080
#define MAX_EVENTS 10

int main(void)
{
    BT_EventLoop eloop;
    bt_eventloop_init(&eloop);

    struct bt_peer peer;
    bt_peer_init(&peer, &eloop, 16);

    if (bt_peer_connect(&peer, LOCALHOST, PORT)) {
        perror("connect()");
        return EXIT_FAILURE;
    }

    bt_peer_choke(&peer);
    bool running = true;

    while (running) {
        struct bt_event ev;
        int evtype = bt_eventloop_run(&eloop, &ev);

        switch (evtype) {
        case BT_EVPEER_CONNECT:
            {
                byte info_hash[SHA1_DIGEST_LEN] = {0};
                byte peer_id[20] = "hello world";
                struct bt_handshake *hsk = bt_handshake(info_hash, peer_id);
                bt_peer_handshake(ev.a, hsk);
            }
            break;
        case BT_EVPEER_DISCONNECT:
            puts("disconnected");
            running = false;
            break;
        }
    }

    return 0;
}
