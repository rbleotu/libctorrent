#pragma once

#include "util/vector.h"

typedef struct bt_diskmgr BT_DiskMgr;

struct bt_file {
    const char *path;
    uint64 sz;
    uint64 off;
    uint64 pos;
    int fd;
};

VECTOR_DEFINE(struct bt_file, File);

struct bt_diskmgr {
    uint64 total_sz;
    Vector(File) files;
};

void bt_disk_init(BT_DiskMgr *m);
int bt_disk_add_file(IN BT_DiskMgr *m, char *path, uint64 sz);
int bt_disk_read(BT_DiskMgr *m, void *data, uint64 base, size_t len);
int bt_disk_write(BT_DiskMgr *m, const void *data, uint64 base, size_t len);
