#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "util/common.h"
#include "util/error.h"
#include "crypt/sha1.h"
#include "piece.h"

void bt_piece_init(BT_Piece p, uint32 len, const byte hash[SHA1_DIGEST_LEN])
{
    p->downloading = false;
    p->length = len;
    memcpy(p->hash, hash, sizeof(p->hash));
    p->freq = 0;
}

void bt_piece_mark_downloading(BT_Piece p)
{
    p->downloading = true;
}

bool bt_piece_isdownloading(BT_Piece p)
{
    return p->downloading;
}

double bt_piece_calcscore(BT_Piece p)
{
    if (!p->freq)
        return 0.f;
    if (p->downloading)
        return 0.f;
    return 1.f;
}

void bt_piece_incrfreq(BT_Piece p)
{
    p->freq++;
}

void bt_piece_decrfreq(BT_Piece p)
{
    p->freq--;
}

bool bt_piece_complete(BT_Piece p)
{
    return p->dlnext == p->length;
}

uint32 bt_piece_dlrover(BT_Piece piece)
{
    return piece->dlnext;
}

void bt_piece_incrdlrover(BT_Piece piece, uint32 n)
{
    n = MIN(n, piece->length - piece->dlnext);
    piece->dlnext += n;
}
