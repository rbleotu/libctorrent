#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <setjmp.h>

#include "common.h"
#include "bcode/bcode.h"
#include "error.h"
#include "crypt/sha1.h"
#include "torrent.h"

struct bt_torrent {
    BT_BCode info;
    uint8_t info_digest[SHA1_DIGEST_LEN];

    const char *outdir;
    const char *announce;

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

extern BT_Torrent
bt_torrent_new(FILE *f, const char *outdir)
{
    assert(f);

    BT_Torrent t = bt_torrent_alloc();
    if (!t)
        return NULL;

    if (bt_bdecode_file(&t->info, f) < 0) {
        bt_free(t);
        return NULL;
    }

    if (!B_ISDICT(t->info)) {
        bt_free(t);
        bt_errno = BT_EFORMAT;
        return NULL;
    }

    return t;
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
