#pragma once

#include <sys/types.h>
#include <unistd.h>

typedef struct bt_diskmgr *BT_DiskMgr;

struct bt_file {
    const char *path;
    off_t sz, off;
    int fd;
};

struct bt_diskmgr {
    size_t nfiles, cap;
    struct bt_file files[];
};

int
bt_disk_add_file(IN BT_DiskMgr m, IN char *path, IN off_t sz);

BT_DiskMgr
bt_disk_new(size_t n);

int
bt_disk_read_piece(IN BT_DiskMgr m, OUT uint8 data[], IN size_t len,
                   IN off_t off);

int
bt_disk_write_piece(IN BT_DiskMgr m, IN uint8 data[], IN size_t len,
                    IN off_t off);
