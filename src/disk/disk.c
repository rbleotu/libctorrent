#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <setjmp.h>

#include "../common.h"
#include "../error.h"
#include "disk.h"

BT_DiskMgr
bt_disk_new(size_t n)
{
    BT_DiskMgr m = bt_malloc(sizeof(*m) + sizeof(m->files[0]) * n);
    if (!m) {
        bt_errno = BT_EALLOC;
        return NULL;
    }
    m->nfiles = 0;
    m->cap = n;
    return m;
}

local int
make_dir_tree(char *path)
{
    char *p = strtok(path, "/");
    while ((p = strtok(NULL, "/"))) {
        if (errno = 0, mkdir(path, 0764) < 0) {
            struct stat sb;
            if (errno != EEXIST)
                goto dir_error;
            if (stat(path, &sb) < 0)
                goto dir_error;
            if (!S_ISDIR(sb.st_mode))
                goto alias_error;
        }
        size_t len = strlen(path);
        memset(path + len, '/', (p - path) - len);
    }
    return 0;

dir_error:
    bt_errno = BT_EMKDIR;
    return -1;

alias_error:
    bt_errno = BT_EDIRALIAS;
    return -1;
}

local int
get_file(struct bt_file *f, char *path)
{
    if (make_dir_tree(path) < 0)
        return -1;

    int fd = open(path, O_RDWR | O_CREAT, 0664);
    if (fd < 0) {
        bt_errno = BT_EOPEN;
        return -1;
    }

    f->path = path;
    f->fd = fd;
    return 0;
}

int
bt_disk_add_file(BT_DiskMgr m, char *path, off_t sz)
{
    assert(path);
    assert(m);
    assert(m->nfiles < m->cap);

    size_t n = m->nfiles;
    m->files[n].sz = sz;
    m->files[n].off = 0;
    if (n)
        m->files[n].off = m->files[n - 1].off + m->files[n - 1].sz;
    if (get_file(&m->files[n], path) < 0)
        return -1;
    m->nfiles++;
    return 0;
}

// int
// main(int argc, char *argv[])
//{
//    if (argc < 2)
//        return 1;
//    struct bt_file f;
//    if (get_file(&f, argv[1]) < 0) {
//        fprintf(stderr, "Error creating dir '%s': %s\n", argv[1],
//                bt_strerror());
//        return 1;
//    }
//    return 0;
//}
