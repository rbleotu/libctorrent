#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "peer.h"
#include "net.h"

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

    bt_eventloop_run(&eloop);
    return 0;
}
