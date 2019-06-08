#ifndef TRACKER_UDP_H_
#define TRACKER_UDP_H_

unsigned
bt_tracker_udp_request(OUT struct tracker_response *resp,
                       IN char domain[], IN uint16_t port,
                       IN struct tracker_request *req);

#endif
