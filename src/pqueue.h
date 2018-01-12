#ifndef PQUEUE_H_
#define PQUEUE_H_

typedef unsigned Key;
typedef unsigned Priority;

#define CMP(a, b) ((a) < (b) ? (-1) : (a) > (b) ? (1) : 0)
#define P_MIN 0

typedef struct bt_pqueue *BT_PQueue;

struct bt_pqueue_entry {
    Key k;
    Priority p;
};


struct bt_pqueue {
    size_t n, cap;
    struct bt_pqueue_entry v[];
};

BT_PQueue
bt_pqueue_new(size_t cap);

int
bt_pqueue_insert(BT_PQueue q, Key k, Priority p);

Key
bt_pqueue_remove(BT_PQueue q);

#define pq_isempty(pq) ((pq)->n == 0)

#define pq_min(pq) ((pq)->v[1].k)


#endif
