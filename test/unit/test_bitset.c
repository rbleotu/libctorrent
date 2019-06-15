#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "util/common.h"
#include "util/error.h"
#include "bitset.h"

int main(void)
{
    BT_Bitset set = bt_bitset_new(128);
    assert (set != NULL);

    bt_bitset_set(set, 0);
    assert (bt_bitset_next0(set) == 1);

    bt_bitset_set(set, 1);
    assert (bt_bitset_next0(set) == 2);

    for (size_t i=0; i<127; i++) {
        bt_bitset_set(set, i);
    }

    assert (bt_bitset_next0(set) == 127);
    return 0;
}
