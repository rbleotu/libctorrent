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
#include <errno.h>

#include "util/common.h"
#include "util/error.h"
#include "torrentx.h"
#include "message.h"
#include "net.h"

int net_tcp_reuseaddr(int sockfd)
{
    int value = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &value, sizeof(int)) < 0) {
        perror("setsockopt()");
        return -1;
    }
    return 0;
}

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

ssize_t net_tcp_send(int sockfd, const void *data, size_t sz)
{
    size_t total = 0;

    while (sz) {
        const ssize_t n = send(sockfd, data, sz, 0);
        if (n <= 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK) {
                break;
            }
            return -1;
        }
        sz -= n, data = (byte *)data + n;
        total += n;
    }

    return total;
}

ssize_t net_tcp_recv(int sockfd, void *data, size_t sz)
{
    size_t total = 0;

    while (sz) {
        const ssize_t n = recv(sockfd, data, sz, 0);
        if (n <= 0) {
            if (errno == EINTR)
                continue;
            if (errno == EWOULDBLOCK)
                break;
            if (n == 0)
                break;
            return -1;
        }
        sz -= n, data = (byte *)data + n;
        total += n;
    }

    return total;
}

int net_tcp_haserror(int sockfd)
{
    int retval = -1;
    socklen_t len = sizeof(retval);

    if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &retval, &len) < 0) {
        perror("getsockopt");
        return -1;
    }

    if (retval == EAGAIN)
        return 0;

    return retval;
}

int net_tcp_ipv4listen(uint16 port)
{
    const int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return -1;
    }

    const struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr = {INADDR_ANY},
    };

    net_tcp_reuseaddr(sockfd);

    const int ec = bind(sockfd, (void *)&addr, sizeof(addr));
    if (ec < 0) {
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, 16) < 0) {
        close(sockfd);
        return -1;
    }

    nonblock(sockfd);
    return sockfd;
}

int net_tcp_accept(int sockfd)
{
    return accept(sockfd, NULL, 0);
}

