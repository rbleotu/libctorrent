#pragma once

int net_tcp_connect(uint32 ipv4, uint16 port);
//int net_tcp_sendmsg(int sockfd, struct bt_msg *msg);
//struct bt_msg  *net_tcp_recvmsg(int sockfd);

ssize_t net_tcp_send(int sockfd, const void *data, size_t sz);
ssize_t net_tcp_recv(int sockfd, void *data, size_t sz);
int net_tcp_disconnect(int sockfd);
int net_tcp_haserror(int sockfd);
int net_tcp_ipv4listen(uint16 port);
int net_tcp_accept(int sockfd);
int net_tcp_reuseaddr(int sockfd);
