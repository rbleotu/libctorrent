#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>

#include "../error.h"
#include "../common.h"
#include "bcode.h"

enum BCODE_ERR {
    B_ENONE,
    B_EREAD,
    B_EALLOC,
    B_ESYNTAX,
    B_EOVERFLOW,

    B_ERR_CNT
};

struct t_bstring {
    size_t len;
    uint8_t content[];
};

struct t_barray {
    size_t n, cap;
    T_BCode base[];
};

static T_BArray
t_barray_new(size_t cap)
{
    T_BArray arr = t_malloc(sizeof(*arr) + sizeof(T_BCode[cap]));
    if (!arr)
        return NULL;
    *arr = (struct t_barray){0, cap};
    return arr;
}

static int
t_barray_resize(T_BArray *arr, size_t newsz)
{
    assert(arr);
    T_BArray tmp = realloc(*arr, sizeof(*tmp) + sizeof(T_BCode[newsz]));
    if (!tmp)
        return -1;
    tmp->cap = newsz;
    *arr = tmp;
    return 0;
}

static T_BCode
t_blist_get(T_BList lst, size_t i)
{
    return lst->base[i];
}

static T_BCode
t_bdict_get(T_BDict dict, uint8_t *str, size_t len)
{
    T_BCode(*v)[2] = (void *)dict->base;
    size_t lt = 0, rt = dict->n / 2;
    while (lt < rt) {
        size_t mid = lt + (rt - lt) / 2;
        int cmp = memcmp(str, B_STR(v[mid][0])->content,
                         MIN(len, B_STR(v[mid][0])->len));
        if (cmp == 0)
            return dict->base[mid + 1];
        else if (cmp > 0)
            lt = mid + 1;
        else
            rt = mid;
    }
    return (T_BCode){BCODE_NONE};
}

#define BUF_SZ (1 << 16)
#define PUSHBACK_SZ (1 << 4)

typedef struct t_bparser *T_BParser;

struct t_bparser {
    FILE *fin;
    uint8_t buf[BUF_SZ];
    const uint8_t *pnext, *pend;

    char pushback[PUSHBACK_SZ];
    int pushed;

    jmp_buf env;
};

static __attribute__((noreturn)) void
throw_error(T_BParser ctx, int err)
{
    switch (err) {
    case B_ENONE:
        t_eprintf(0, "success\n");
        break;
    case B_EREAD:
        t_eprintf(T_EBCODE, "error reading bcoded file\n");
        break;
    case B_EALLOC:
        t_eprintf(T_EBCODE, "error allocating memory\n");
        break;
    case B_ESYNTAX:
        t_eprintf(T_EBCODE, "syntax error\n");
        break;
    case B_EOVERFLOW:
        t_eprintf(T_EBCODE, "integer overflow\n");
        break;
    default:
        assert("bad error code\n" && 0);
    }
    longjmp(ctx->env, err);
}


static size_t
sc_refill_buffer(T_BParser ctx)
{
    if (!ctx->fin)
        return 0;
    size_t xfer = fread(ctx->buf, 1, BUF_SZ, ctx->fin);
    if (!xfer) {
        if (ferror(ctx->fin))
            throw_error(ctx, B_EREAD);
        return 0;
    }
    ctx->pend = ctx->buf;
    ctx->pend = ctx->buf + xfer;
    return xfer;
}

static int
sc_get(T_BParser ctx)
{
    if (ctx->pushed)
        return ctx->pushback[--ctx->pushed];

    if (ctx->pnext >= ctx->pend) {
        if (sc_refill_buffer(ctx) == 0)
            return EOF;
    }

    return *ctx->pnext++;
}

static inline void
sc_unget(T_BParser ctx, int ch)
{
    assert(ctx->pushed < PUSHBACK_SZ);

    ctx->pushback[ctx->pushed++] = ch;
}

#define EXPECT(ctx, c)                     \
    do {                                   \
        int tmp_ = sc_get(ctx);            \
        if (tmp_ != c)                     \
            throw_error((ctx), B_ESYNTAX); \
    } while (0)

static T_BInt
sc_read_num(T_BParser ctx)
{
    int c;
    T_BInt x = 0;
    while (isdigit(c = sc_get(ctx)))
        x = 10 * x + (c - '0');
    sc_unget(ctx, c);
    return x;
}

static T_BString
t_bstring_new(size_t len)
{
    T_BString str = t_malloc(sizeof(*str) + len);
    if (!str)
        return NULL;
    str->len = len;
    return str;
}

static int
sc_read_string(T_BParser ctx, uint8_t dest[], size_t len)
{
    size_t have;
    have = ctx->pend - ctx->pnext;
    have = MIN(len, have);
    memcpy(dest, ctx->pnext, have);

    while (len -= have) {
        have = sc_refill_buffer(ctx);
        have = MIN(len, have);
        if (!have)
            return -1;
        memcpy(dest, ctx->buf, have);
    }

    ctx->pnext += have;
    return 0;
}

static T_BString
parse_string(T_BParser ctx)
{
    T_BInt len = sc_read_num(ctx);
    if (!len)
        throw_error(ctx, B_ESYNTAX);
    EXPECT(ctx, ':');
    T_BString ret = t_bstring_new(len);
    if (!ret)
        throw_error(ctx, B_EALLOC);
    int s = sc_read_string(ctx, ret->content, len);
    if (s < 0)
        throw_error(ctx, B_ESYNTAX);
    return ret;
}

static T_BCode
parse(T_BParser ctx);

static T_BList
parse_list(T_BParser ctx)
{
#define INIT_SZ 8
    T_BList lst = t_barray_new(INIT_SZ);
    if (!lst)
        throw_error(ctx, B_EALLOC);
    size_t n = 0, sz = INIT_SZ;
    for (int c; (c = sc_get(ctx)) != 'e';) {
        if (c == EOF)
            throw_error(ctx, B_ESYNTAX);
        sc_unget(ctx, c);
        lst->base[n++] = parse(ctx);
        if (n >= sz) {
            sz <<= 1;
            if (t_barray_resize(&lst, sz) < 0)
                throw_error(ctx, B_ESYNTAX);
        }
    }
    lst->n = n;
    return lst;
}

static T_BDict
parse_dict(T_BParser ctx)
{
    T_BDict dict = t_barray_new(INIT_SZ);
    if (!dict)
        throw_error(ctx, B_EALLOC);
    size_t n = 0, sz = INIT_SZ;
    for (int c; (c = sc_get(ctx)) != 'e';) {
        if (c == EOF)
            throw_error(ctx, B_ESYNTAX);
        sc_unget(ctx, c);
        dict->base[n++] = (T_BCode){BCODE_STRING, {.str = parse_string(ctx)}};
        dict->base[n++] = parse(ctx);
        if (n >= sz) {
            sz <<= 1;
            if (t_barray_resize(&dict, sz) < 0)
                throw_error(ctx, B_ESYNTAX);
        }
    }
    dict->n = n;
    return dict;
}

static T_BCode
parse(T_BParser ctx)
{
    assert(ctx);
    T_BCode ret = {BCODE_NONE};

    int c = sc_get(ctx);

    /* clang-format off */
    switch (c) {
    case 'i':
        ret.type = BCODE_INT;
        B_NUM(ret) = sc_read_num(ctx);
        EXPECT(ctx, 'e');
        break;
    case 'l':
        ret.type = BCODE_LIST;
        B_LST(ret) = parse_list(ctx);
        break;
    case 'd':
        ret.type = BCODE_DICT;
        B_DICT(ret) = parse_dict(ctx);
        break;
    case '1': case '2': case '3':
    case '4': case '5': case '6':
    case '7': case '8': case '9':
        sc_unget(ctx, c);
        ret.type = BCODE_STRING;
        B_STR(ret) = parse_string(ctx);
        break;
    default:
        throw_error(ctx, B_ESYNTAX);
    }
    /* clang-format on */

    return ret;
}

extern int
t_bdecode_file(FILE *f, T_BCode *res)
{
    assert(res);

    struct t_bparser ctx = {.fin = f, .pnext = ctx.buf, .pend = ctx.buf};
    if (setjmp(ctx.env))
        return -1;
    *res = parse(&ctx);
    return 0;
}

extern int
t_bdecode_str(const uint8_t *str, size_t len, T_BCode *res)
{
    struct t_bparser ctx = {.fin = NULL, .pnext = str, .pend = str + len};
    if (setjmp(ctx.env))
        return -1;
    *res = parse(&ctx);
    return 0;
}

extern int
t_bencode_file(FILE *f, T_BCode b)
{
    return 0;
}

extern void
t_bcode_fprint(FILE *f, T_BCode b)
{
    switch (b.type) {
    case BCODE_INT:
        fprintf(f, "%ld", B_NUM(b));
        break;
    case BCODE_STRING:
        fputc('"', f);
        fwrite(B_STR(b)->content, 1, B_STR(b)->len, f);
        fputc('"', f);
        break;
    case BCODE_LIST:
        fputc('[', f);
        for (size_t i = 0; i < B_LST(b)->n; i++) {
            t_bcode_fprint(f, B_LST(b)->base[i]);
            if (i + 1 < B_LST(b)->n)
                fputc(',', f);
        }
        fputc(']', f);
        break;
    case BCODE_DICT:
        fputc('{', f);
        for (size_t i = 0; i < B_DICT(b)->n; i += 2) {
            t_bcode_fprint(f, B_DICT(b)->base[i]);
            fputc(':', f);
            t_bcode_fprint(f, B_DICT(b)->base[i + 1]);
            if (i + 2 < B_DICT(b)->n)
                fputc(',', f);
        }
        fputc('}', f);
        break;
    }
}

static T_BCode
t_bcode_get(T_BCode b, ...)
{
    va_list ap;
    va_start(ap, b);
    switch (b.type) {
    case BCODE_INT:
    case BCODE_STRING:
        return b;
        break;
    case BCODE_LIST:
        return t_blist_get(B_LST(b), va_arg(ap, int));
    case BCODE_DICT: {
        uint8_t *str = va_arg(ap, uint8_t *);
        size_t len = va_arg(ap, size_t);
        return t_bdict_get(B_DICT(b), str, len);
    }
    }
    va_end(ap);
}
