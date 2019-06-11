#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util/common.h"
#include "util/error.h"
#include "message.h"
#include "net.h"

#define LOCALHOST ((127<<24) | 1)
#define PORT      8080
#define MAX_EVENTS 10

int main(void)
{
    const int sockfd = net_tcp_connect(LOCALHOST, PORT);

    int n;
    struct epoll_event ev, events[MAX_EVENTS];
    int listen_sock, conn_sock, nfds, epollfd;

    /* Code to set up listening socket, 'listen_sock',
       (socket(), bind(), listen()) omitted */

    epollfd = epoll_create1(0);
    if (epollfd == -1) {
        perror("epoll_create1");
        exit(EXIT_FAILURE);
    }

    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.fd = sockfd;
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sockfd, &ev) == -1) {
        perror("epoll_ctl: listen_sock");
        exit(EXIT_FAILURE);
    }

    for (;;) {
        nfds = epoll_wait(epollfd, events, MAX_EVENTS, -1);
        if (nfds == -1) {
            perror("epoll_wait");
            exit(EXIT_FAILURE);
        }

        for (n = 0; n < nfds; ++n) {
            if (events[n].data.fd == sockfd) {
                const ssize_t nxfer = send(sockfd, "hello world", 12, 0);
                if (nxfer < 12) {
                    perror("send");
                    return EXIT_FAILURE;
                }
                puts("ok");
            }
        }
    }

    return 0;
}
