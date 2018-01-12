#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "error.h"
#include "pqueue.h"

BT_PQueue
bt_pqueue_new(size_t cap)
{
    BT_PQueue q = malloc(sizeof(*q) + sizeof(q->v[0]) * ++cap);
    if (!q) {
        bt_errno = BT_EALLOC;
        return NULL;
    }
    q->v[0].p = P_MIN;
    q->n = 0;
    q->cap = cap;
    return q;
}

local void
upheap(struct bt_pqueue_entry v[], size_t n)
{
    struct bt_pqueue_entry x = v[n];
    for (; CMP(v[n / 2].p, v[n].p) > 0; n /= 2)
        v[n] = v[n / 2];
    v[n] = x;
}

local void
downheap(struct bt_pqueue_entry v[], size_t i, size_t n)
{
    struct bt_pqueue_entry x = v[i];
    for (size_t j; (j = 2 * i) <= n; i = j) {
        if (j < n && CMP(v[j].p, v[j + 1].p) > 0)
            j++;
        if (CMP(v[i].p, v[j].p) <= 0)
            break;
        v[i] = v[j];
    }
    v[i] = x;
}

int
bt_pqueue_insert(BT_PQueue q, Key k, Priority p)
{
    assert(q);
    if (q->n >= q->cap)
        return -1;
    q->v[++q->n] = (struct bt_pqueue_entry){k, p};
    upheap(q->v, q->n);
    return 0;
}

Key
bt_pqueue_remove(BT_PQueue q)
{
    assert(q && q->n);
    Key ret = q->v[1].k;
    q->v[1] = q->v[q->n--];
    downheap(q->v, 1, q->n);
    return ret;
}

void
bt_pqueue_fprint(FILE *f, BT_PQueue q)
{
    assert(q && f);
    putc('[', f);
    for (size_t i = 1; i <= q->n;) {
        fprintf(f, "(%u,%u)", q->v[i].k, q->v[i].p);
        fputc(++i <= q->n ? ';' : ']', f);
    }
    putc('\n', f);
}

// int
// main(int argc, char *argv[])
//{
//    BT_PQueue q = bt_pqueue_new(10);
//    bt_pqueue_insert(q, 10, 3);
//    bt_pqueue_insert(q, 20, 2);
//    bt_pqueue_insert(q, 20, 2);
//    bt_pqueue_insert(q, 20, 100);
//    bt_pqueue_fprint(stdout, q);
//    bt_pqueue_remove(q);
//    bt_pqueue_remove(q);
//    bt_pqueue_fprint(stdout, q);
//    return 0;
//}
