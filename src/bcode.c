#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <stdint.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>

#include "util/error.h"
#include "bcode.h"

local BArray
barray_new(size_t cap)
{
    BArray arr = bt_malloc(sizeof(*arr) + sizeof(BT_BCode[cap]));
    if (!arr)
        return NULL;
    *arr = (struct barray){0, cap};
    return arr;
}

local int
barray_resize(BArray *arr, size_t newsz)
{
    assert(arr);
    BArray tmp =
            realloc(*arr, sizeof(*tmp) + sizeof(BT_BCode[newsz]));
    if (!tmp)
        return -1;
    tmp->cap = newsz;
    *arr = tmp;
    return 0;
}

local BT_BCode
blist_get(BT_BList lst, size_t i)
{
    return lst->base[i];
}

local int
bstring_cmp(const uint8_t *a, size_t n, const uint8_t *b, size_t m)
{
    return memcmp(a, b, MIN(n, m));
}

local BT_BCode
bdict_get(BT_BDict dict, const uint8_t *str, size_t len)
{
    BT_BCode(*v)[2] = (void *)dict->base;
    size_t lt = 0, rt = dict->n / 2;
    while (lt < rt) {
        size_t mid = lt + (rt - lt) / 2;
        int cmp = bstring_cmp(b_string(v[mid][0]),
                              v[mid][0].u.string->len, str, len);
        if (cmp == 0)
            return v[mid][1];
        else if (cmp < 0)
            lt = mid + 1;
        else
            rt = mid;
    }
    return (BT_BCode){BCODE_NONE};
}

#define BUF_SZ (1 << 16)
#define PUSHBACK_SZ (1 << 4)

typedef struct t_bparser *BT_BParser;

struct t_bparser {
    FILE *fin;
    uint8_t buf[BUF_SZ];
    const uint8_t *pnext, *pend;

    char pushback[PUSHBACK_SZ];
    int pushed;

    jmp_buf env;
};

local __attribute__((noreturn)) void
throw_error(BT_BParser ctx, int err)
{
    bt_errno = err;
    longjmp(ctx->env, err);
}

local size_t
sc_refill_buffer(BT_BParser ctx)
{
    if (!ctx->fin)
        return 0;
    size_t xfer = fread(ctx->buf, 1, BUF_SZ, ctx->fin);
    if (!xfer) {
        if (ferror(ctx->fin))
            throw_error(ctx, BT_EREAD);
        return 0;
    }
    ctx->pnext = ctx->buf;
    ctx->pend = ctx->buf + xfer;
    return xfer;
}

local int
sc_get(BT_BParser ctx)
{
    if (ctx->pushed)
        return ctx->pushback[--ctx->pushed];

    if (ctx->pnext >= ctx->pend) {
        if (sc_refill_buffer(ctx) == 0)
            return EOF;
    }

    return *ctx->pnext++;
}

local inline void
sc_unget(BT_BParser ctx, int ch)
{
    assert(ctx->pushed < PUSHBACK_SZ);

    ctx->pushback[ctx->pushed++] = ch;
}

#define EXPECT(ctx, c)                      \
    do {                                    \
        int tmp_ = sc_get(ctx);             \
        if (tmp_ != c)                      \
            throw_error((ctx), BT_ESYNTAX); \
    } while (0)

local BT_BInt
sc_read_num(BT_BParser ctx)
{
    BT_BInt x = 0;
    int neg = 0, c;
    if ((c = sc_get(ctx)) == '0')
        return 0;
    if (c == '-') {
        neg = 1;
        c = sc_get(ctx);
    }
    if (!isdigit(c))
        throw_error(ctx, BT_ESYNTAX);
    do {
        BT_BInt tmp = ((BT_BInt)10) * x + (BT_BInt)(c - '0');
        if (tmp < x)
            throw_error(ctx, BT_EOVERFLOW);
        x = tmp;
    } while (isdigit(c = sc_get(ctx)));
    if (neg)
        x = -x;
    sc_unget(ctx, c);
    return x;
}

local BT_BString
t_bstring_new(size_t len)
{
    BT_BString str = bt_malloc(sizeof(*str) + len + 1);
    if (!str)
        return NULL;
    str->len = len;
    return str;
}

local int
sc_read_string(BT_BParser ctx, uint8_t dest[], size_t len)
{
#ifdef BT_DEBUG
    while (len--) {
        int c = sc_get(ctx);
        if (c == EOF)
            return -1;
        *dest++ = c;
    }
#else
    assert(ctx->pushed == 0);
    size_t have;
    have = ctx->pend - ctx->pnext;
    have = MIN(len, have);
    memcpy(dest, ctx->pnext, have);

    while (dest += have, len -= have) {
        have = sc_refill_buffer(ctx);
        have = MIN(len, have);
        if (!have)
            return -1;
        memcpy(dest, ctx->buf, have);
    }
    ctx->pnext += have;
#endif
    return 0;
}

local BT_BString
parse_string(BT_BParser ctx)
{
    BT_BInt len = sc_read_num(ctx);
    EXPECT(ctx, ':');
    BT_BString ret = t_bstring_new(len);
    if (!ret)
        throw_error(ctx, BT_EALLOC);
    int s = sc_read_string(ctx, ret->content, len);
    if (s < 0)
        throw_error(ctx, BT_ESYNTAX);
    ret->content[len] = '\0';
    return ret;
}

local BT_BCode
parse(BT_BParser ctx);

local BT_BList
parse_list(BT_BParser ctx)
{
#define INIBT_SZ 8
    BT_BList lst = barray_new(INIBT_SZ);
    if (!lst)
        throw_error(ctx, BT_EALLOC);
    size_t n = 0, sz = INIBT_SZ;
    for (int c; (c = sc_get(ctx)) != 'e';) {
        if (c == EOF)
            throw_error(ctx, BT_EEOF);
        sc_unget(ctx, c);
        lst->base[n++] = parse(ctx);
        if (n >= sz) {
            if (barray_resize(&lst, sz <<= 1) < 0)
                throw_error(ctx, BT_ESYNTAX);
        }
    }
    lst->n = n;
    barray_resize(&lst, n);
    return lst;
}

local BT_BDict
parse_dict(BT_BParser ctx)
{
    BT_BDict dict = barray_new(INIBT_SZ);
    if (!dict)
        throw_error(ctx, BT_EALLOC);
    size_t n = 0, sz = INIBT_SZ;
    for (int c; (c = sc_get(ctx)) != 'e';) {
        if (c == EOF) {
            throw_error(ctx, BT_EEOF);
        }
        sc_unget(ctx, c);
        dict->base[n++] = (BT_BCode){BCODE_STRING,
                                     {.string = parse_string(ctx)}};
        dict->base[n++] = parse(ctx);
        if (n >= sz) {
            if (barray_resize(&dict, sz <<= 1) < 0)
                throw_error(ctx, BT_ESYNTAX);
        }
    }
    dict->n = n;
    barray_resize(&dict, n);
    return dict;
}

local BT_BCode
parse(BT_BParser ctx)
{
    assert(ctx);
    BT_BCode ret = {BCODE_NONE};

    int c = sc_get(ctx);

    /* clang-format off */
    switch (c) {
    case 'i':
        ret.id = BCODE_INT;
        b_num(ret) = sc_read_num(ctx);
        EXPECT(ctx, 'e');
        break;
    case 'l':
        ret.id= BCODE_LIST;
        b_list(ret) = parse_list(ctx);
        break;
    case 'd':
        ret.id = BCODE_DICT;
        b_dict(ret) = parse_dict(ctx);
        break;
    case '0':
    case '1': case '2': case '3':
    case '4': case '5': case '6':
    case '7': case '8': case '9':
        sc_unget(ctx, c);
        ret.id = BCODE_STRING;
        ret.u.string = parse_string(ctx);
        break;
    default:
        throw_error(ctx, BT_ESYNTAX);
    }
    /* clang-format on */

    return ret;
}

extern int
bt_bdecode_file(BT_BCode *res, FILE *f)
{
    assert(res);

    struct t_bparser ctx = {
            .fin = f, .pnext = ctx.buf, .pend = ctx.buf};
    if (setjmp(ctx.env))
        return -1;
    *res = parse(&ctx);
    return 0;
}

extern int
bt_bdecode_str(BT_BCode *res, const uint8 *str, size_t len)
{
    struct t_bparser ctx = {
            .fin = NULL, .pnext = str, .pend = str + len};
    if (setjmp(ctx.env))
        return -1;
    *res = parse(&ctx);
    return 0;
}

extern int
bt_bencode_file(FILE *f, BT_BCode b)
{
    return 0;
}

local void
add_indent(FILE *f, unsigned indent)
{
#if !__STRICT_ANSI__
    static const char buf[] = {[0 ...(1 << 9)] = ' '};
#else
    static const char buf[] = {
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
            ' '};
#endif
    while (indent > sizeof(buf)) {
        fwrite(buf, sizeof(buf), 1, f);
        indent -= sizeof(buf);
    }
    fwrite(buf, indent, 1, f);
}

local void
bstring_fprint(FILE *f, BT_BString s)
{
    fputc('"', f);
    size_t len = s->len;
    uint8_t *data = s->content;
    for (; len--; data++) {
        if (*data == '"' || *data == '\\')
            fprintf(f, "\\%c", (char)*data);
        else if (isprint(*data))
            fputc(*data, f);
        else
            fprintf(f, "\\x%02x", (int)*data);
    }
    fputc('"', f);
}

#define INDENT_LVL 4

local void
bcode_fprint(FILE *f, BT_BCode b, unsigned indent)
{
    switch (b.id) {
    case BCODE_INT:
        fprintf(f, "%ld", b_num(b));
        break;
    case BCODE_STRING:
        bstring_fprint(f, b.u.string);
        break;
    case BCODE_LIST:
        fputc('[', f);
        for (size_t i = 0; i < b_list(b)->n; i++) {
            if (i)
                add_indent(f, indent + INDENT_LVL);
            bcode_fprint(f, b_list(b)->base[i],
                         i ? indent + INDENT_LVL : 1);
            if (i + 1 < b_list(b)->n)
                fputs(",\n", f);
        }
        fputc(']', f);
        break;
    case BCODE_DICT:
        fputc('{', f);
        for (size_t i = 0; i < b_dict(b)->n; i += 2) {
            if (i)
                add_indent(f, indent + INDENT_LVL);
            bcode_fprint(f, b_dict(b)->base[i],
                         i ? indent + INDENT_LVL : 1);
            fputc(':', f);
            bcode_fprint(f, b_dict(b)->base[i + 1],
                         indent + INDENT_LVL);
            if (i + 2 < b_dict(b)->n)
                fputs(",\n", f);
        }
        fputc('}', f);
        break;
    }
}

extern void
bt_bcode_fprint(FILE *f, BT_BCode b)
{
    bcode_fprint(f, b, 0);
    fputc('\n', f);
}

extern BT_BCode
bt_bcode_get(BT_BCode b, ...)
{
    va_list ap;
    va_start(ap, b);
    switch (b.id) {
    case BCODE_INT:
    case BCODE_STRING:
        return b;
        break;
    case BCODE_LIST:
        return blist_get(b_list(b), va_arg(ap, int));
    case BCODE_DICT: {
        uint8_t *str = va_arg(ap, uint8_t *);
        size_t len = strlen((char *)str);
        return bdict_get(b_dict(b), str, len);
    }
    default:
        assert(0);
    }
    va_end(ap);
    return (BT_BCode){.id = BCODE_NONE};
}

typedef struct bencoder *BEncoder;

struct bencoder {
    uint8_t *dest;
    size_t rem;
};

local size_t
bencode_array(uint8 str[], size_t len, BT_BCode v[], size_t n)
{
    size_t res = 0;
    for (size_t tmp, i = 0; i < n; ++i) {
        tmp = bt_bencode(str, len, v[i]);
        res += tmp;
        tmp = MIN(tmp, len);
        str += tmp, len -= tmp;
    }
    return res;
}

extern size_t
bt_bencode(uint8_t dest[], size_t len, BT_BCode b)
{
    size_t res = 0;
    char tmp[32];
    switch (b.id) {
    case BCODE_INT: {
        res = snprintf(tmp, sizeof(tmp), "i%lde", b_num(b));
        if (dest)
            memcpy(dest, tmp, MIN(len, res));
        return res;
    }
    case BCODE_STRING: {
        size_t slen = b.u.string->len;
        res = snprintf(tmp, sizeof(tmp), "%zu:", slen);
        if (dest) {
            memcpy(dest, tmp, MIN(len, res));
            dest += MIN(len, res), len -= MIN(len, res);
            memcpy(dest, b_string(b), MIN(len, slen));
        }
        return res + slen;
    }
    case BCODE_LIST: {
        if (dest && len--)
            *dest++ = 'l';
        res = bencode_array(dest, len, b_list(b)->base, b_list(b)->n);
        if (dest && len--)
            dest[res] = 'e';
        return res + 2;
    }
    case BCODE_DICT: {
        if (dest && len--)
            *dest++ = 'd';
        res = bencode_array(dest, len, b_dict(b)->base, b_dict(b)->n);
        if (dest && len--)
            dest[res] = 'e';
        return res + 2;
    } break;
    default:
        assert(0);
    }
    return 0;
}

void
bt_bcode_free(BT_BCode b)
{
    BArray a;
    switch (b.id) {
    case BCODE_INT:
        return;
    case BCODE_STRING:
        free(b.u.string);
        return;
    case BCODE_DICT:
        a = b_dict(b);
        break;
    case BCODE_LIST:
        a = b_list(b);
        break;
    default:
        assert(0);
    }
    for (size_t i = 0; i < a->n; i++)
        bt_bcode_free(a->base[i]);
    free(a);
}

// int
// main(int argc, char *argv[])
//{
//    FILE *f = freopen(argv[1], "rb", stdin);
//    if (!f) {
//        fprintf(stderr, "Failed to open '%s' : %s\n", argv[1],
//                strerror(errno));
//        return 1;
//    }
//    BT_BCode b;
//    if (bt_bdecode_file(&b, f) < 0) {
//        fprintf(stderr, "Error decoding: %s\n", bt_strerror());
//        return 1;
//    }
//
//    size_t len = bt_bencode(NULL, 0, b);
//    uint8_t *dest = malloc(len);
//    if (!dest) {
//        perror("malloc");
//        return 1;
//    }
//    bt_bencode(dest, len, b);
//    fwrite(dest, 1, len, stdout);
//
//    return 0;
//}
