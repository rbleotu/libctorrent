#ifndef BITSET_H_
#define BITSET_H_

typedef struct bt_bitset *BT_Bitset;

BT_Bitset
bt_bitset_new(size_t n);

void
bt_bitset_set(BT_Bitset b, size_t i);

void
bt_bitset_clear(BT_Bitset b, size_t i);

void
bt_bitset_fprint(FILE *to, BT_Bitset set);

void
bt_bitset_read_arr(BT_Bitset b, uint8 v[], size_t sz);

int
bt_bitset_get(BT_Bitset set, size_t i);

size_t bt_bitset_next0(BT_Bitset set);

#endif
