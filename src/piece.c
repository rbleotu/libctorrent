#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "util/common.h"
#include "util/error.h"
#include "crypt/sha1.h"
#include "piece.h"

void bt_piece_init(BT_Piece p, uint64 off, uint32 len, const byte hash[SHA1_DIGEST_LEN])
{
    p->length = len;
    memcpy(p->hash, hash, sizeof(p->hash));
    p->freq = 0;

    uint32 nblocks = CEIL_DIV(len, CHUNK_SZ);
    p->off = off;
    p->have = bt_bitset_new(nblocks);
    p->requested = bt_bitset_new(nblocks);
    p->nblocks = nblocks;
    p->nhave = 0;
}

int bt_piece_allocblock(BT_Piece piece)
{
    for (size_t i=0; i < piece->nblocks; i++) {
        if (bt_bitset_get(piece->have, i))
            continue;
        if (bt_bitset_get(piece->requested, i))
            continue;
        bt_bitset_set(piece->requested, i);
        return i;
    }

    return -1;
}

void bt_piece_unallocblock(BT_Piece piece, uint32 i)
{
    bt_bitset_clear(piece->requested, i);
}

void bt_piece_markhave(BT_Piece piece, uint32 i)
{
    if (bt_bitset_get(piece->have, i))
        return;
    bt_bitset_set(piece->have, i);
    piece->nhave++;
}
