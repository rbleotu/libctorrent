#ifndef DISK_H_
#define DISK_H_

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

    jmp_buf env;

    struct bt_file files[];
};

int
bt_disk_add_file(IN BT_DiskMgr m, IN char *path, IN off_t sz);

BT_DiskMgr
bt_disk_new(size_t n);

int
bt_disk_get_piece(IN BT_DiskMgr, OUT u8 data[], IN size_t len,
                  IN off_t off);

int
bt_disk_put_piece(IN BT_DiskMgr, IN const u8 *data, IN size_t len,
                  IN off_t off);

#endif
