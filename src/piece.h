#ifndef PIECE_H_
#define PIECE_H_

#include <pthread.h>
#include "crypt/sha1.h"
#include "bitset.h"

#define CHUNK_SZ (1 << 14)
typedef struct bt_piece *BT_Piece;

struct bt_piece {
    off_t off;
    size_t length;

    uint8 hash[SHA1_DIGEST_LEN];

    volatile int freq;
    volatile int have;
    BT_Bitset chunks;

    pthread_mutex_t lock;
    uint8 *data;
};

int
bt_piece_init(BT_Piece p, off_t off, size_t len);

size_t
bt_piece_empty_chunk(BT_Piece p);

int
bt_piece_add_chunk(BT_Piece p, size_t i, uint8 data[]);

int
bt_piece_check(BT_Piece p);

void
bt_piece_fprint(FILE *to, BT_Piece p);

int
bt_piece_load(BT_Piece p, BT_DiskMgr m);

int
bt_piece_save(BT_Piece p, BT_DiskMgr m);

#endif
