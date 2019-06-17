#pragma once

#include "util/common.h"

struct txbuf {
    byte buf[1<<14];

    const byte *next;
    const byte *end;

    int (*request_next)(struct txbuf *this);
};

local inline bool txbuf_isempty(struct txbuf *buf) {
    return buf->end - buf->next == 0;
}

local inline const byte *txbuf_next(struct txbuf *buf) {
    return buf->next;
}

local inline size_t txbuf_left(struct txbuf *buf) {
    return buf->end - buf->next;
}

local inline int txbuf_refill(struct txbuf *buf) {
    return buf->request_next(buf);
}
