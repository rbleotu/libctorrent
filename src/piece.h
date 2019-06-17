#pragma once

#include "crypt/sha1.h"
#include "bitset.h"

#define CHUNK_SZ (1 << 14)
typedef struct bt_piece *BT_Piece;

struct bt_piece {
    byte hash[SHA1_DIGEST_LEN];
    uint64 off;
    uint32 length;
    uint32 nblocks;
    uint32 nhave;

    int freq;

    BT_Bitset have;
    BT_Bitset requested;
};

void bt_piece_init(BT_Piece piece, uint64 off, uint32 length, const byte digest[SHA1_DIGEST_LEN]);
int bt_piece_allocblock(BT_Piece piece);
void bt_piece_unallocblock(BT_Piece piece, uint32 i);
void bt_piece_markhave(BT_Piece piece, uint32 i);

static inline void bt_piece_incrfreq(BT_Piece piece)
{
    ++piece->freq;
}

static inline void bt_piece_decrfreq(BT_Piece piece)
{
    --piece->freq;
}

static inline bool bt_piece_complete(BT_Piece piece)
{
    return piece->nhave == piece->nblocks;
}

static inline size_t bt_piece_blocklength(BT_Piece piece, uint32 block)
{
    if (block < piece->nblocks - 1)
        return CHUNK_SZ;
    return CHUNK_SZ - (piece->length % CHUNK_SZ);
}

static inline size_t bt_piece_blockoffset(BT_Piece piece, uint32 block)
{
    return block * CHUNK_SZ;
}

static inline uint32 bt_piece_blockfromoff(uint32 off)
{
    return off / CHUNK_SZ;
}

static inline double bt_piece_progress(BT_Piece piece)
{
    return (double)piece->nhave / piece->nblocks;
}
