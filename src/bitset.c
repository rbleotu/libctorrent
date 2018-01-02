#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"

typedef struct bt_bitset *BT_BitSet;

struct bt_bitset {
    size_t len;
    uint32_t bits[];
};

#define CEIL_DIV(a, b) (((a)-1) / (b) + 1)

BT_BitSet
bt_bitset_new(size_t n)
{
    size_t len = CEIL_DIV(n, 32);
    BT_BitSet set = malloc(sizeof(*set) + sizeof(uint32_t[len]));
    if (!set)
        return NULL;
    set->len = len;
    memset(set->bits, 0, sizeof(int[len]));
    return set;
}

void
bt_bitset_set(BT_BitSet set, size_t i)
{
    assert(set);
    set->bits[i >> 5] |= 1 << (i ^ 31);
}

void
bt_bitset_clear(BT_BitSet set, size_t i)
{
    assert(set);
    set->bits[i >> 5] &= ~(1 << (i ^ 31));
}

int
bt_bitset_get(BT_BitSet set, size_t i)
{
    assert(set);
    return set->bits[i >> 5] & (1 << (i ^ 31));
}

local void
uint32_fprint(FILE *to, uint32_t x)
{
    for (uint32_t i = ~(~0u >> 1); i; i >>= 1)
        fputc(x & i ? '1' : '0', to);
}

void
bt_bitset_fprint(FILE *to, BT_BitSet set)
{
    for (size_t i = 0; i < set->len; i++)
        uint32_fprint(to, set->bits[i]);
    fputc('\n', to);
}

// int
// main(int argc, char *argv[])
//{
//    BT_BitSet s = bt_bitset_new(10);
//    bt_bitset_set(s, 9);
//    bt_bitset_set(s, 10);
//    bt_bitset_fprint(stdout, s);
//    bt_bitset_clear(s, 9);
//    bt_bitset_set(s, 0);
//    bt_bitset_set(s, 31);
//    bt_bitset_fprint(stdout, s);
//    return 0;
//}
