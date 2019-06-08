#ifndef COMMON_H_
#define COMMON_H_

#include <stdint.h>

#define bt_malloc malloc
#define bt_free free

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define CEIL_DIV(a, b) (((a)-1) / (b) + 1)

#define SWAP(a, b, tmp) ((tmp) = (a), (a) = (b), (b) = (tmp))

#define PUT_U16BE(m, u) ((m)[0] = ((u) >> 8), (m)[1] = (u)&255u)
#define PUT_U32BE(m, u)                               \
    ((m)[0] = (u) >> 24, (m)[1] = ((u) >> 16) & 255u, \
     (m)[2] = ((u) >> 8) & 255u, (m)[3] = (u)&255u)
#define PUT_U64BE(m, u)           \
    (PUT_U32BE((m), ((u) >> 32)), \
     PUT_U32BE(((m) + 4), ((u)&0xffffffffLU)))

#define GET_U16BE(m, u) ((u) = (m)[0], (u) = ((u) << 8) | (m)[1])
#define GET_U32BE(m, u)                       \
    ((u) = (m)[0], (u) = ((u) << 8) | (m)[1], \
     (u) = ((u) << 8) | (m)[2], (u) = ((u) << 8) | (m)[3])
#define GET_U64BE(m, u)                                    \
    ((u) = (m)[0], (u) = ((u) << 8) | (m)[1],              \
     (u) = ((u) << 8) | (m)[2], (u) = ((u) << 8) | (m)[3], \
     (u) = ((u) << 8) | (m)[4], (u) = ((u) << 8) | (m)[5], \
     (u) = ((u) << 8) | (m)[6], (u) = ((u) << 8) | (m)[7])

#define ARR_LEN(a) (sizeof(a) / sizeof((a)[0]))

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

struct bt_peer_addr {
    uint32 ipv4;
    uint16 port;
};

#define IN
#define OUT

#define local static

#endif
