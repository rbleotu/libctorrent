#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include <string.h>
#include <stdarg.h>

#include "util/common.h"
#include "bcode.h"
#include "util/error.h"
#include "crypt/sha1.h"
#include "torrent.h"
#include "disk.h"
#include "tracker.h"
#include "piece.h"
#include "thread.h"
#include "torrentx.h"

local inline unsigned
hex_value(char h)
{
    /* clang-format off */
    return h >= '0' && h <= '9' ? h - '0' :
           h >= 'a' && h <= 'f' ? h - 'a' + 10:
           h >= 'A' && h <= 'F' ? h - 'A' + 10: 0;
    /* clang-format on */
}

local size_t
hex_decode(uint8_t dest[], size_t dlen, const char *src, size_t slen)
{
    for (size_t i = 0; slen--; *dest <<= 4, i++) {
        *dest |= hex_value(*src++);
        if (i % 2) {
            if (!dlen--)
                break;
            ++dest;
        }
    }
    return dlen;
}

local BT_Torrent
bt_torrent_alloc(void)
{
    BT_Torrent t = bt_malloc(sizeof(*t));
    if (!t) {
        bt_errno = BT_EALLOC;
        return NULL;
    }
    t->naddr = 0;
    t->addrtab = NULL;
    t->npieces = 0;
    t->piece_length = 0;
    t->nhave = 0;
    t->size = 0;
    t->mgr = NULL;
    return t;
}

#define format_check(cond)     \
    do {                       \
        if (!(cond))           \
            goto format_error; \
    } while (0)

#define safe_realloc(buf, sz)              \
    do {                                   \
        void *tmp_ = realloc((buf), (sz)); \
        if (!tmp_) {                       \
            bt_errno = BT_EALLOC;          \
            goto alloc_error;              \
        }                                  \
        (buf) = tmp_;                      \
    } while (0)

local char *
vjoin_strings(size_t *sz, va_list ap)
{
    assert(sz);
    char *res = NULL;
    size_t i = 0, len;
    for (char *str; (str = va_arg(ap, char *)); i += len) {
        len = strlen(str);
        safe_realloc(res, i + len + 1);
        memcpy(res + i, str, len);
    }
    *sz = i + 1;
    res[i] = '\0';
    return res;
alloc_error:
    free(res);
    return NULL;
}

local char *
build_path(BT_BCode b, ...)
{
#define INIT_SZ 16
#define INCR_SZ 32
    assert(b_islist(b));

    va_list ap;
    char *res;
    size_t i = 0, sz = 0;
    BT_BCode s;

    va_start(ap, b);
    res = vjoin_strings(&sz, ap);
    va_end(ap);

    if (!res)
        return NULL;

    i = sz - 1;

    for (size_t n = 0; !b_isnil(s = bt_bcode_get(b, n++));) {
        if (!b_isstring(s))
            break;
        const size_t len = s.u.string->len;
        format_check(b_isstring(s));
        res[i++] = '/';
        if ((i += len) + 1 >= sz) {
            sz = i + 1 + INCR_SZ;
            safe_realloc(res, sz);
        }
        memcpy(res + i - len, b_string(s), len);
    }

    res[i] = '\0';
    return realloc(res, i + 1);
alloc_error:
    bt_errno = BT_EALLOC;
    return NULL;
format_error:
    bt_free(res);
    bt_errno = BT_EFORMAT;
    return NULL;
#undef INIT_SZ
#undef INCR_SZ
}


local int
init_pieces(BT_Torrent t, BT_BCode pieces)
{
    assert(t);
    assert(b_isstring(pieces));

    size_t plen = t->piece_length;
    size_t rem = t->size;
    size_t n = b_strlen(pieces) / SHA1_DIGEST_LEN;
    uint8_t(*hash_tab)[SHA1_DIGEST_LEN] = (void *)b_string(pieces);

    for (BT_Piece i = t->piecetab; rem; ++i) {
        if (!n) {
            bt_errno = BT_EFORMAT;
            return -1;
        }
        bt_piece_init(i, t->size - rem, MIN(rem, plen));
        memcpy(i->hash, *hash_tab, SHA1_DIGEST_LEN);
        n -= SHA1_DIGEST_LEN, ++hash_tab;
        rem -= i->length;
    }
    return 0;
}

local size_t
get_trackers(const char *dest[], BT_BCode meta)
{
    size_t n = 0;
    BT_BCode announce = bt_bcode_get(meta, "announce");
    if (b_isstring(announce))
        dest[n++] = (char *)b_string(announce);
    BT_BCode anlst = bt_bcode_get(meta, "announce-list");
    if (b_islist(anlst)) {
        BT_BCode tier, tracker;
        for (size_t i = 0; i < b_list(anlst)->n; i++) {
            tier = bt_bcode_get(anlst, i);
            if (!b_islist(tier))
                continue;
            for (size_t j = 0; j < b_list(tier)->n; j++) {
                tracker = bt_bcode_get(tier, j);
                if (b_isstring(tracker)) {
                    assert(n < MAX_TRACKERS);
                    dest[n++] = (void *)b_string(tracker);
                }
            }
        }
    }
    dest[n] = NULL;
    return n;
}


local int
compute_hash(uint8_t hash[], BT_BCode b)
{
    size_t len = bt_bencode(NULL, 0, b);
    uint8_t *str = bt_malloc(len);
    if (!str) {
        bt_errno = BT_EALLOC;
        return -1;
    }
    bt_bencode(str, len, b);
    bt_sha1(hash, str, len);
    bt_free(str);
    return 0;
}

local int
from_metainfo_file(BT_Torrent t, const char *path)
{
    BT_BCode meta, info, files;
    BT_BCode name, piece_length, pieces;

    const char * const outdir = t->settings.outdir;
    const char *name_str;
    off_t sz;

    FILE *f = fopen(path, "rb");
    if (!f) {
        bt_errno = BT_EOPEN;
        return -1;
    }

    if (bt_bdecode_file(&meta, f) < 0)
        goto error;

    format_check(b_isdict(meta));

    info = bt_bcode_get(meta, "info");
    format_check(b_isdict(info));

    name = bt_bcode_get(info, "name");
    format_check(b_isstring(name));

    log_info(t, "metainfo.name: '%s'", (char *)b_string(name));

    piece_length = bt_bcode_get(info, "piece length");
    format_check(b_isnum(piece_length));

    log_info(t, "metainfo.plen: %uB", b_num(piece_length));

    pieces = bt_bcode_get(info, "pieces");
    format_check(b_isstring(pieces));

    t->piece_length = b_num(piece_length);

    t->mgr = bt_disk_new(30);
    if (!t->mgr)
        goto error;

    if (b_islist(files = bt_bcode_get(info, "files"))) {
        t->size = 0;
        BT_BCode file, path;
        char *path_str;
        for (size_t i = 0; !b_isnil(file = bt_bcode_get(files, i));
             i++) {
            format_check(b_isdict(file));

            path = bt_bcode_get(file, "path");
            format_check(b_islist(path));

            if (!(path_str = build_path(path, outdir, b_string(name), NULL)))
                goto error;

            sz = b_num(bt_bcode_get(file, "length"));
            t->size += sz;

            log_info(t, "file: %30s size: %8u", path, sz);

            if (bt_disk_add_file(t->mgr, path_str, sz) < 0)
                goto error;
        }
    } else {
        char path[256];
        sz = b_num(bt_bcode_get(info, "length"));

        snprintf(path, sizeof(path), "%s/%s", outdir, b_string(name));

        log_info(t, "file: %30s size: %8u", b_string(name), sz);

        if (bt_disk_add_file(t->mgr, path, sz) < 0)
            goto error;
    }

    t->npieces = CEIL_DIV(t->size, t->piece_length);

    safe_realloc(t, sizeof(*t) + sizeof(struct bt_piece[t->npieces]));

    if (init_pieces(t, pieces) < 0)
        goto error;

    size_t ntrackers = get_trackers(t->announce, meta);
    if (!ntrackers) {
        bt_errno = BT_ENOTRACKER;
        goto error;
    }

    log_info(t, "tracker count: %u", (unsigned)ntrackers);

    if (compute_hash(t->info_hash, info) < 0)
        goto error;

    fclose(f);
    return 0;
alloc_error:
format_error:
error:
    fclose(f);
    return -1;
}

extern BT_Torrent
bt_torrent_new(BT_Settings settings)
{
    assert (settings != NULL);


    BT_Torrent t = bt_torrent_alloc();
    if (!t)
        return NULL;

    t->settings = *settings;

    if (settings->metainfo_path) {
        log_info(t, "reading metainfo from file '%s'", settings->metainfo_path);
        if (from_metainfo_file(t, settings->metainfo_path))
            goto error;
    } else {
        assert (!"TODO: magnet links");
    }

    return t;
error:
    bt_free(t);
    return NULL;
}

extern int
bt_torrent_tracker_request(BT_Torrent t, unsigned npeer)
{
    assert(t);
    assert(t->announce);

    struct tracker_request req;
    struct tracker_response resp;
    unsigned nhave;
    const char **l = t->announce;

    safe_realloc(t->addrtab,
                 sizeof(t->addrtab[0]) * (t->naddr + npeer));

    req = (struct tracker_request){
        .downloaded = 0,
        .left       = t->size - t->size_have,
        .uploaded   = 0,
        .num_want   = npeer,
        .event      = 0,
        .port       = t->settings.port
    };
    resp = (struct tracker_response){.peertab =
                                             t->addrtab + t->naddr};
    memcpy(req.info_hash, t->info_hash, SHA1_DIGEST_LEN);

    for (nhave = 0; *l; ++l) {
        puts(*l);
        if (nhave == npeer)
            break;
        unsigned n = bt_tracker_request(&resp, *l, &req);
        resp.peertab += n, req.num_want -= n;
        nhave += n;
    }


    return t->naddr += nhave;
alloc_error:
    return -1;
}

extern void
bt_torrent_add_action(BT_Torrent t, int ev, void *h)
{
}

extern int
bt_torrent_start(BT_Torrent t)
{
    assert(t);
    if (!t->naddr) {
        bt_errno = BT_ENOPEERS;
        return -1;
    }
    //bt_net_start(t);
    return 0;
}

extern int
bt_torrent_pause(BT_Torrent t)
{
    return 0;
}

extern int
bt_torrent_check(BT_Torrent t)
{
    assert(t && t->mgr);

    uint8_t hash[SHA1_DIGEST_LEN];

    uint8_t *data = bt_malloc(t->piece_length);
    if (!data) {
        bt_errno = BT_EALLOC;
        return -1;
    }

    for (unsigned i = 0; i < t->npieces; i++) {
        if (bt_disk_read_piece(t->mgr, data, t->piecetab[i].length,
                               t->piecetab[i].off) < 0)
            return -1;
        bt_sha1(hash, data, t->piecetab[i].length);
        if (check_hash(hash, t->piecetab[i].hash)) {
            t->piecetab[i].have = 1;
            t->size_have += t->piecetab[i].length;
            t->nhave++;
        }
    }

    free(data);
    return 0;
}

extern unsigned
bt_torrent_get_nhave(BT_Torrent t)
{
    return t->nhave;
}
