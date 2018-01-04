#ifndef BITSET_H_
#define BITSET_H_

typedef struct bt_bitset *BT_Bitset;

BT_Bitset
bt_bitset_new(size_t n);

void
bt_bitset_set(T_Bitset b, size_t i);

void
bt_bitset_clear(T_Bitset b, size_t i);

#endif