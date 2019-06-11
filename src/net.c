#include <fcntl.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <poll.h>
#include <pthread.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "util/common.h"
#include "util/error.h"
#include "torrentx.h"
#include "message.h"
#include "net.h"

local int nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    assert (flags != -1);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int net_tcp_connect(uint32 ipv4, uint16 port)
{
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {htonl(ipv4)},
    };

    nonblock(sockfd);
    connect(sockfd, (struct sockaddr *) &addr, sizeof(addr));
    return sockfd;
}

int net_tcp_disconnect(int sockfd)
{
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}
