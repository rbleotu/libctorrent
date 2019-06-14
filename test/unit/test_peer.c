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
#include "peerserver.h"

#define LOCALHOST ((127<<24) | 1)
#define PORT      8080
#define MAX_EVENTS 10

struct ipv4addr {
    uint32 ip;
    uint16 port;
};

local int
parse_addr(const char str[], struct ipv4addr *addr)
{
    byte ip[4];
    int ec = sscanf(str, "%hhu.%hhu.%hhu.%hhu:%hu", &ip[0], &ip[1], &ip[2], &ip[3], &addr->port);
    if (ec != 5)
        return -1;
    GET_U32BE(ip, addr->ip);
    return 0;
}

int main(int argc, char *argv[])
{
    bool is_server = argv[1] ? !strcmp(argv[1], "-s") : false;
    BT_EventLoop eloop;
    bt_eventloop_init(&eloop);

    BT_PeerServer server;
    bt_peerserver_init(&server, &eloop, PORT);

    struct bt_peer peer;
    bt_peer_init(&peer, &eloop, 16);

    if (is_server) {
        if (bt_peerserver_listen(&server)) {
            perror("listen()");
            return EXIT_FAILURE;
        }
    } else {
        struct ipv4addr a = {LOCALHOST, PORT};
        if (argv[1]) {
            if (parse_addr(argv[1], &a)) {
                fputs("bad ipv4 address format", stderr);
                return EXIT_FAILURE;
            }
        }

        if (bt_peer_connect(&peer, a.ip, a.port)) {
            perror("connect()");
            return EXIT_FAILURE;
        }
    }

    bool running = true;

    while (running) {
        //struct bt_event ev;
        //int evtype = bt_eventloop_run(&eloop, &ev);

        //switch (evtype) {
        //case BT_EVPEER_CONNECT:
        //    {
        //        byte info_hash[SHA1_DIGEST_LEN] =
        //            "\xcf\x5c\x59\x53\x19\xbf\x85\x47\x5a\xce\x0c\xf6\x67\xf4\xf7\xda\x58\xef\xa8\x92";
        //        byte peer_id[20] = "hello world";
        //        struct bt_handshake *hsk = bt_handshake(info_hash, peer_id);
        //        bt_peer_handshake(ev.a, hsk);
        //    }
        //    break;
        //case BT_EVPEER_HANDSHAKE:
        //    {

        //    }
        //    break;
        //case BT_EVPEER_DISCONNECT:
        //    puts("disconnected");
        //    running = false;
        //    break;
        //}
    }

    return 0;
}
