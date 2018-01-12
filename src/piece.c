#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "error.h"
#include "crypt/sha1.h"
#include "disk/disk.h"
#include "piece.h"

int
bt_piece_init(BT_Piece p, off_t off, size_t len)
{
    p->off = off;
    p->length = len;
    p->have = 0;
    p->freq = 0;
    p->chunks = bt_bitset_new(CEIL_DIV(len, CHUNK_SZ));
    p->data = NULL;
    pthread_mutex_init(&p->lock, NULL);
    return p->chunks ? 0 : -1;
}

size_t
bt_piece_empty_chunk(BT_Piece p)
{
    size_t i = 0, nc = CEIL_DIV(p->length, CHUNK_SZ);
    for (; i < nc; i++)
        if (!bt_bitset_get(p->chunks, i))
            return i;
    return i;
}

int
bt_piece_add_chunk(BT_Piece p, size_t i, u8 data[])
{
    size_t off = i * CHUNK_SZ;
    size_t chlen = MIN(off + CHUNK_SZ, p->length) - off;
    bt_bitset_set(p->chunks, i);
    if (!p->data) {
        p->data = bt_malloc(p->length);
        if (!p->data) {
            bt_errno = BT_EALLOC;
            return -1;
        }
    }
    memcpy(p->data + off, data, chlen);
    size_t n = 0, nc = CEIL_DIV(p->length, CHUNK_SZ);
    for (; n < nc; n++)
        if (!bt_bitset_get(p->chunks, n))
            return 0;
    p->have = 1;
    return 0;
}

int
bt_piece_check(BT_Piece p)
{
    assert(p->data);
    u8 hash[SHA1_DIGEST_LEN];
    bt_sha1(hash, p->data, p->length);
    if (!memcmp(hash, p->data, SHA1_DIGEST_LEN))
        return 0;
    return -1;
}

void
bt_piece_fprint(FILE *to, BT_Piece p)
{
    putc('[', to);
    size_t nc = CEIL_DIV(p->length, CHUNK_SZ);
    for (size_t i = 0; i < nc; i++) {
        if (bt_bitset_get(p->chunks, i))
            putc('|', to);
        else
            putc(' ', to);
    }
    fputs("]\n", to);
}

// int
// bt_piece_check(BT_DiskMgr m, BT_Piece p)
//{
//    // uint8_t hash[SHA1_DIGEST_LEN];
//    // uint8_t *data = bt_disk_get_piece(m, p);
//    // if (!data)
//    //    return -1;
//    // bt_sha1(hash, data, p->length);
//    // int cmp = memcmp(hash, p->hash, SHA1_DIGEST_LEN);
//    // free(data);
//    return 0;
//}
