#ifndef MESSAGE_H_
#define MESSAGE_H_

#include "crypt/sha1.h"
#include "util/common.h"

#define HANDLE_EVENT(t,p,e,c) case (BT_##e): return HANDLE_##e((t), (p), (c), (data))
#define HANDLE_EVPEER_CONNECT(t,p,c,d)       (c)(t,p)
#define HANDLE_EVPEER_HANDSHAKE(t,p,c,d)     (c)(t,p,(struct bt_handshake *)(d))
#define HANDLE_EVPEER_DISCONNECT(t,p,c,d)    (c)(t,p)
#define HANDLE_EVPEER_HAVE(t,p,c,d)          (c)(t,p,((struct bt_msg_have *)(d))->piecei)
#define HANDLE_EVPEER_CHOKE(t,p,c,d)         (c)(t,p)
#define HANDLE_EVPEER_UNCHOKE(t,p,c,d)       (c)(t,p)
#define HANDLE_EVPEER_INTERESTED(t,p,c,d)    (c)(t,p)
#define HANDLE_EVPEER_NOTINTERESTED(t,p,c,d) (c)(t,p)
#define HANDLE_EVPEER_KEEPALIVE(t,p,c,d)     (c)(t,p)
#define HANDLE_EVPEER_BITFIELD(t,p,c,d)      (c)(t,p,((struct bt_msg_bitfield *)(d))->bitfield, ((struct bt_msg_bitfield *)(d))->len)
#define HANDLE_EVPEER_PIECE(t,p,c,d) \
    (c)(t,p,((struct bt_msg_piece *)d)->block, ((struct bt_msg_piece *)d)->piecei, ((struct bt_msg_piece *)d)->begin, ((struct bt_msg_piece *)d)->len - 9)

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

    BT_MSG_CNT,
    BT_MSG_HANDSHAKE,
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
#define bt_msg_len(msg) (((struct bt_msg *)(msg))->len + 4)

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

struct bt_handshake {
    uint32 len;
    uint8 id;
    byte magic[20];
    byte reserved[8];
    byte info_hash[SHA1_DIGEST_LEN];
    byte peer_id[20];
} __attribute__((packed));

struct bt_msg *
bt_msg_new(int id, ...);

#define HSHK_LEN 68
#define HSHK_FILL(d, ih, p)                                 \
    do {                                                    \
        memcpy((d).magic, "\x13""BitTorrent protocol", 20); \
        memcpy((d).info_hash, (ih), sizeof((d).info_hash)); \
        memcpy((d).peer_id, (p), sizeof((d).peer_id));      \
    } while (0)

void
bt_msg_free(struct bt_msg *msg);

struct bt_msg *
bt_msg_unpack(const uint8 buf[]);

void
bt_msg_pack(byte dest[], struct bt_msg *msg);

void bt_msg_pack(byte buf[], struct bt_msg *);

static inline struct bt_handshake *
bt_handshake(byte info_hash[SHA1_DIGEST_LEN], byte peer_id[20])
{
    struct bt_handshake *hsk = malloc(sizeof(*hsk));
    if (!hsk) {
        return NULL;
    }
    memset(hsk, 0, sizeof(*hsk));
    HSHK_FILL(*hsk, info_hash, peer_id);
    hsk->len = sizeof(*hsk) - 9;
    hsk->id = BT_MSG_HANDSHAKE;
    return hsk;
}

struct bt_handshake *bt_handshake_unpack(const byte src[]);

#endif
