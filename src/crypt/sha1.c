#include <stdint.h>
#include <string.h>

#include "sha1.h"

#define GET_U32BE(x)                                       \
    (((uint32_t)(x)[0] << 24) | ((uint32_t)(x)[1] << 16) | \
     ((uint32_t)(x)[2] << 8) | (uint32_t)(x)[3])

#define PUT_U64BE(d, x)                                      \
    ((d)[0] = ((x) >> 56), (d)[1] = ((x) >> 48) & 255,       \
     (d)[2] = ((x) >> 40) & 255, (d)[3] = ((x) >> 32) & 255, \
     (d)[4] = ((x) >> 24) & 255, (d)[5] = ((x) >> 16) & 255, \
     (d)[6] = ((x) >> 8) & 255, (d)[7] = (x)&255)

#define PUT_U32BE(d, x)                                \
    ((d)[0] = ((x) >> 24), (d)[1] = ((x) >> 16) & 255, \
     (d)[2] = ((x) >> 8) & 255, (d)[3] = (x)&255)

/* clang-format off */
static const uint8_t padding[64] =
{0x80, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00,
 0x00, 0x00, 0x00, 0x00,0x00,0x00,0x00,0x00};
/* clang-format on */

static inline uint32_t
lt_rot(uint32_t x, int n)
{
    return (x << n) | (x >> (32 - n));
}

#define W_FUNC(w, i) \
    lt_rot((w)[(i)-3] ^ (w)[(i)-8] ^ (w)[(i)-14] ^ (w)[(i)-16], 1)

static void
sha1_round(uint32_t h[], const uint8_t *chunk)
{
    uint32_t W[80];
    uint32_t A = h[0];
    uint32_t B = h[1];
    uint32_t C = h[2];
    uint32_t D = h[3];
    uint32_t E = h[4];
    uint32_t temp;

    W[0] = GET_U32BE(chunk);
    W[1] = GET_U32BE(chunk + 4);
    W[2] = GET_U32BE(chunk + 8);
    W[3] = GET_U32BE(chunk + 12);

    W[4] = GET_U32BE(chunk + 16);
    W[5] = GET_U32BE(chunk + 20);
    W[6] = GET_U32BE(chunk + 24);
    W[7] = GET_U32BE(chunk + 28);

    W[8] = GET_U32BE(chunk + 32);
    W[9] = GET_U32BE(chunk + 36);
    W[10] = GET_U32BE(chunk + 40);
    W[11] = GET_U32BE(chunk + 44);

    W[12] = GET_U32BE(chunk + 48);
    W[13] = GET_U32BE(chunk + 52);
    W[14] = GET_U32BE(chunk + 56);
    W[15] = GET_U32BE(chunk + 60);

#define S(a, n) (lt_rot((a), (n)))

#define R(t)                                                             \
    (temp = W[(t - 3) & 0x0F] ^ W[(t - 8) & 0x0F] ^ W[(t - 14) & 0x0F] ^ \
            W[t & 0x0F],                                                 \
     (W[t & 0x0F] = S(temp, 1)))
#define P(a, b, c, d, e, x)                \
    {                                      \
        e += S(a, 5) + F(b, c, d) + K + x; \
        b = S(b, 30);                      \
    }

#define F(x, y, z) (z ^ (x & (y ^ z)))
#define K 0x5A827999

    P(A, B, C, D, E, W[0]);
    P(E, A, B, C, D, W[1]);
    P(D, E, A, B, C, W[2]);
    P(C, D, E, A, B, W[3]);
    P(B, C, D, E, A, W[4]);
    P(A, B, C, D, E, W[5]);
    P(E, A, B, C, D, W[6]);
    P(D, E, A, B, C, W[7]);
    P(C, D, E, A, B, W[8]);
    P(B, C, D, E, A, W[9]);
    P(A, B, C, D, E, W[10]);
    P(E, A, B, C, D, W[11]);
    P(D, E, A, B, C, W[12]);
    P(C, D, E, A, B, W[13]);
    P(B, C, D, E, A, W[14]);
    P(A, B, C, D, E, W[15]);
    P(E, A, B, C, D, R(16));
    P(D, E, A, B, C, R(17));
    P(C, D, E, A, B, R(18));
    P(B, C, D, E, A, R(19));

#undef K
#undef F

#define F(x, y, z) (x ^ y ^ z)
#define K 0x6ED9EBA1

    P(A, B, C, D, E, R(20));
    P(E, A, B, C, D, R(21));
    P(D, E, A, B, C, R(22));
    P(C, D, E, A, B, R(23));
    P(B, C, D, E, A, R(24));
    P(A, B, C, D, E, R(25));
    P(E, A, B, C, D, R(26));
    P(D, E, A, B, C, R(27));
    P(C, D, E, A, B, R(28));
    P(B, C, D, E, A, R(29));
    P(A, B, C, D, E, R(30));
    P(E, A, B, C, D, R(31));
    P(D, E, A, B, C, R(32));
    P(C, D, E, A, B, R(33));
    P(B, C, D, E, A, R(34));
    P(A, B, C, D, E, R(35));
    P(E, A, B, C, D, R(36));
    P(D, E, A, B, C, R(37));
    P(C, D, E, A, B, R(38));
    P(B, C, D, E, A, R(39));

#undef K
#undef F

#define F(x, y, z) ((x & y) | (z & (x | y)))
#define K 0x8F1BBCDC

    P(A, B, C, D, E, R(40));
    P(E, A, B, C, D, R(41));
    P(D, E, A, B, C, R(42));
    P(C, D, E, A, B, R(43));
    P(B, C, D, E, A, R(44));
    P(A, B, C, D, E, R(45));
    P(E, A, B, C, D, R(46));
    P(D, E, A, B, C, R(47));
    P(C, D, E, A, B, R(48));
    P(B, C, D, E, A, R(49));
    P(A, B, C, D, E, R(50));
    P(E, A, B, C, D, R(51));
    P(D, E, A, B, C, R(52));
    P(C, D, E, A, B, R(53));
    P(B, C, D, E, A, R(54));
    P(A, B, C, D, E, R(55));
    P(E, A, B, C, D, R(56));
    P(D, E, A, B, C, R(57));
    P(C, D, E, A, B, R(58));
    P(B, C, D, E, A, R(59));

#undef K
#undef F

#define F(x, y, z) (x ^ y ^ z)
#define K 0xCA62C1D6

    P(A, B, C, D, E, R(60));
    P(E, A, B, C, D, R(61));
    P(D, E, A, B, C, R(62));
    P(C, D, E, A, B, R(63));
    P(B, C, D, E, A, R(64));
    P(A, B, C, D, E, R(65));
    P(E, A, B, C, D, R(66));
    P(D, E, A, B, C, R(67));
    P(C, D, E, A, B, R(68));
    P(B, C, D, E, A, R(69));
    P(A, B, C, D, E, R(70));
    P(E, A, B, C, D, R(71));
    P(D, E, A, B, C, R(72));
    P(C, D, E, A, B, R(73));
    P(B, C, D, E, A, R(74));
    P(A, B, C, D, E, R(75));
    P(E, A, B, C, D, R(76));
    P(D, E, A, B, C, R(77));
    P(C, D, E, A, B, R(78));
    P(B, C, D, E, A, R(79));

#undef K
#undef F

    h[0] += A;
    h[1] += B;
    h[2] += C;
    h[3] += D;
    h[4] += E;
}

extern void
t_sha1(uint8_t digest[], const uint8_t *msg, size_t len)
{
    uint8_t end[64];
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476,
                     0xC3D2E1F0};
    uint64_t blen = len << 3;

    while (len >= 64) {
        sha1_round(h, msg);
        len -= 64, msg += 64;
    }

    memcpy(end, msg, len);

    if (len < 56) {
        memcpy(end + len, padding, 56 - len);
    } else {
        memcpy(end + len, padding, 64 - len);
        sha1_round(h, end);
        memset(end, 0u, 56);
    }

    PUT_U64BE(end + 56, blen);
    sha1_round(h, end);

    PUT_U32BE(digest, h[0]);
    PUT_U32BE(digest + 4, h[1]);
    PUT_U32BE(digest + 8, h[2]);
    PUT_U32BE(digest + 12, h[3]);
    PUT_U32BE(digest + 16, h[4]);
}
