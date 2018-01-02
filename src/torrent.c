#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>
#include <string.h>

#include "common.h"
#include "bcode/bcode.h"
#include "error.h"
#include "crypt/sha1.h"
#include "torrent.h"
#include "disk/disk.h"

struct bt_torrent {
    BT_BCode info;

    uint8_t info_digest[SHA1_DIGEST_LEN];

    const char *outdir;
    const char *announce;

    BT_DiskMgr mgr;

    jmp_buf env;
};

local BT_Torrent
bt_torrent_alloc(void)
{
    BT_Torrent t = bt_malloc(sizeof(*t));
    if (!t) {
        bt_errno = BT_EALLOC;
        return NULL;
    }
    return t;
}

#define format_check(cond)     \
    do {                       \
        if (!(cond))           \
            goto format_error; \
    } while (0)

local char *
path_from_list(BT_BCode b)
{
#define INIT_SZ 16
#define INCR_SZ 32
    assert(B_ISLIST(b));
    char *res = bt_malloc(INIT_SZ);
    if (!res)
        goto alloc_error;
    size_t i = 0, sz = INIT_SZ;
    BT_BCode s;
    for (size_t n = 0; !B_ISNIL(s = bt_bcode_get(b, n)); n++) {
        format_check(B_ISSTRING(s));
        if (i + B_STRING(s)->len + 2 >= sz) {
            sz = i + B_STRING(s)->len + 2 + INCR_SZ;
            char *tmp = realloc(res, sz);
            if (!tmp) {
                bt_free(res);
                goto alloc_error;
            }
            res = tmp;
        }
        memcpy(res + i, B_STRING(s)->content, B_STRING(s)->len);
        i += B_STRING(s)->len;
        if (!B_ISNIL(bt_bcode_get(b, n + 1)))
            res[i++] = '/';
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

extern BT_Torrent
bt_torrent_new(FILE *f, const char *outdir)
{
    assert(f);
    char *path;
    BT_BCode info, files, name;
    off_t sz;

    BT_Torrent t = bt_torrent_alloc();
    if (!t)
        return NULL;

    if (bt_bdecode_file(&t->info, f) < 0) {
        bt_free(t);
        return NULL;
    }

    format_check(B_ISDICT(t->info));

    info = bt_bcode_get(t->info, "info");
    format_check(B_ISDICT(info));

    name = bt_bcode_get(info, "name");
    format_check(B_ISSTRING(name));

    t->mgr = bt_disk_new(30);
    if (!t->mgr)
        goto cleanup;

    if (B_ISLIST(files = bt_bcode_get(info, "files"))) {
        for (size_t i = 0;; i++) {
            BT_BCode tmp = bt_bcode_get(files, i);
            if (B_ISNIL(tmp))
                break;
            format_check(B_ISDICT(tmp));

            tmp = bt_bcode_get(tmp, "path");
            format_check(B_ISLIST(tmp));

            if (!(path = path_from_list(tmp)))
                goto cleanup;

            sz = B_NUM(
                    bt_bcode_get(bt_bcode_get(files, i), "length"));
            printf("path:%s size:%ld\n", path, sz);

            if (bt_disk_add_file(t->mgr, path, sz) < 0)
                goto cleanup;
        }
    } else {
        path = (char *)B_STRING(name)->content;
        sz = B_STRING(name)->len;
        if (bt_disk_add_file(t->mgr, path, sz) < 0)
            goto cleanup;
    }

    if (bt_disk_check(t->t->piecetab, t->npieces) < 0)
        goto cleanup;

    return t;

format_error:
    bt_errno = BT_EFORMAT;
    return NULL;

cleanup:
    bt_free(t);
    return NULL;
}

extern int
bt_torrent_tracker_request(BT_Torrent t)
{
    return 0;
}

extern void
bt_torrent_add_action(BT_Torrent t, int ev, void *h)
{
}

extern int
bt_torrent_start(BT_Torrent t)
{
    return 0;
}

extern int
bt_torrent_pause(BT_Torrent t)
{
    return 0;
}
