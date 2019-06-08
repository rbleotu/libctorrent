#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "crypt/sha1.h"
#include "util/common.h"

enum {
    BT_MCHOKE,
    BT_MUNCHOKE,
    BT_MINTERESTED,
    BT_MNOT_INTERESTED,
    BT_MHAVE,
    BT_MBITFIELD,
    BT_MREQUEST,
    BT_MPIECE,
    BT_MCANCEL,
    BT_MPORT,

    BT_MKEEP_ALIVE,

    BT_MSG_CNT
};

local const char *msg_names[] = {
        [BT_MCHOKE] = "BT_MCHOKE",
        [BT_MUNCHOKE] = "BT_MUNCHOKE",
        [BT_MINTERESTED] = "BT_MINTERESTED",
        [BT_MNOT_INTERESTED] = "BT_MNOT_INTERESTED",
        [BT_MHAVE] = "BT_MHAVE",
        [BT_MBITFIELD] = "BT_MBITFIELD",
        [BT_MREQUEST] = "BT_MREQUEST",
        [BT_MPIECE] = "BT_MPIECE",
        [BT_MCANCEL] = "BT_MCANCEL",
        [BT_MPORT] = "BT_MPORT",
        [BT_MKEEP_ALIVE] = "BT_MKEEP_ALIVE",
};

#define BT_MSG_LEN 5
struct bt_msg {
    uint32 len;
    int8 id;
};

struct bt_msg_have {
    uint32 len;
    uint8 id;
    uint32 piecei;
};

struct bt_msg_bitfield {
    uint32 len;
    uint8 id;
    uint8 bitfield[];
};

struct bt_msg_request {
    uint32 len;
    uint8 id;
    uint32 piecei;
    uint32 begin;
    uint32 length;
};

struct bt_msg_piece {
    uint32 len;
    uint8 id;
    uint32 piecei;
    uint32 begin;
    uint8 block[];
};

struct bt_msg_handshake {
    uint8 pstrlen;
    uint8 pstr[19];
    uint8 reserved[8];
    uint8 info_hash[SHA1_DIGEST_LEN];
    uint8 peer_id[20];
};

struct bt_msg *
bt_msg_new(int id, ...);

#define HSHK_FILL(d, ih, p)                                 \
    do {                                                    \
        (d).pstrlen = 19;                                   \
        memcpy((d).pstr, "BitTorrent protocol", 19);        \
        memcpy((d).info_hash, (ih), sizeof((d).info_hash)); \
        memcpy((d).peer_id, (p), sizeof((d).peer_id));      \
    } while (0)

int
bt_handshake_recv(struct bt_msg_handshake *dest, int sockfd);

int
bt_handshake_send(const struct bt_msg_handshake *src, int sockfd);

struct bt_msg *
bt_msg_recv(int sockfd);

int
bt_msg_send(int sockfd, int id, ...);

void
bt_msg_free(struct bt_msg *msg);

struct bt_msg *
bt_msg_unpack(const uint8 buf[]);
#endif
