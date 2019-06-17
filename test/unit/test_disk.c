#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <setjmp.h>

#include "util/common.h"
#include "util/error.h"
#include "disk.h"

int main(void)
{
    BT_DiskMgr mgr;

    bt_disk_init(&mgr);
    bt_disk_add_file(&mgr, "test/a.txt", 1);
    bt_disk_add_file(&mgr, "test/b.txt", 2);
    bt_disk_add_file(&mgr, "test/c.txt", 3);

    bt_disk_write(&mgr, "hello world", 0, sizeof("hello world") - 1);

    return 0;
}
