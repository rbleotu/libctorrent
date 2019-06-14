#ifndef PIECE_H_
#define PIECE_H_

#include "crypt/sha1.h"

#define CHUNK_SZ (1 << 14)
typedef struct bt_piece *BT_Piece;

struct bt_piece {
    uint32 length;
    uint32 dlnext;

    byte hash[SHA1_DIGEST_LEN];

    int freq;
    bool downloading;
};

void bt_piece_init(BT_Piece piece, uint32 length, const byte digest[SHA1_DIGEST_LEN]);
void bt_piece_mark_downloading(BT_Piece piece);
bool bt_piece_isdownloading(BT_Piece piece);
double bt_piece_calcscore(BT_Piece piece);
void bt_piece_incrfreq(BT_Piece piece);
void bt_piece_decrfreq(BT_Piece piece);
bool bt_piece_complete(BT_Piece piece);
uint32 bt_piece_dlrover(BT_Piece piece);
void bt_piece_incrdlrover(BT_Piece piece, uint32);

#endif
