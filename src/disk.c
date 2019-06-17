#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <pthread.h>

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

void
bt_disk_init(BT_DiskMgr *m)
{
    m->total_sz = 0;
    m->files = (Vector(File)) VECTOR();
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

local void
file_set_pos(struct bt_file *file, uint64 pos)
{
    file->pos = pos;
}

int
bt_disk_add_file(BT_DiskMgr *m, char *path, uint64 sz)
{
    assert(path != NULL);
    assert(m != NULL);

    struct bt_file file = {
        .path = path,
        .sz = sz,
        .off = m->total_sz,
        .fd = -1,
        .pos = 0,
    };

    if (vector_pushback(&m->files, file) < 0) {
        perror("malloc");
        return -1;
    }

    m->total_sz += sz;
    return 0;
}

local struct bt_file *
file_from_offset(Vector(File) *v, uint64 off)
{
    size_t i = 0;
    VECTOR_FOREACH(v, file, struct bt_file) {
        if (file->off > off) {
            assert (i != 0);
            return file - 1;
        }
        i = i + 1;
    }

    if (v->n) {
        struct bt_file *last = &v->data[v->n-1];
        if (off < last->off + last->sz) {
            return last;
        }
    }

    return NULL;
}

local ssize_t
readn(int fd, void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nread;
    char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0; /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            break; /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (n - nleft); /* return >= 0 */
}

int
bt_disk_read(BT_DiskMgr *m, void *data, uint64 base, size_t len)
{
    assert (!"not implemented");
    //assert(m != NULL);

    //size_t i = file_from_offset(&m->files, base);
    //ssize_t nread, nxfer;

    //off -= m->files[i].off;

    //for (; len; off = 0, i++) {
    //    assert(i < m->nfiles);

    //    if (lseek(m->files[i].fd, off, SEEK_SET) == -1) {
    //        bt_errno = BT_ELSEEK;
    //        return -1;
    //    }

    //    size_t fsz = m->files[i].sz - off;
    //    nread = MIN(fsz, len);
    //    if ((nxfer = readn(m->files[i].fd, data, nread)) < 0) {
    //        bt_errno = BT_EREAD;
    //        return -1;
    //    }
    //    if (nxfer < nread) {
    //        memset(data + nxfer, 0, nread - nxfer);
    //        nread = len;
    //    }

    //    len -= nread, data += nread;
    //}

    return 0;
}

local int
alloc_fd(const char *path)
{
    return open(path, O_RDWR | O_CREAT, 0764);
}

local ssize_t
writen(int fd, const void *vptr, size_t n)
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = vptr;
    nleft = n;
    while (nleft > 0) {
        if ((nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0; /* and call write() again */
            else
                return (-1); /* error */
        }

        nleft -= nwritten;
        ptr += nwritten;
    }
    return (n);
}

local ssize_t
file_write(struct bt_file *file, const void *data, size_t len)
{
    if (file->fd < 0) {
        file->fd = alloc_fd(file->path);
        if (file->fd < 0) {
            perror("open");
            return -1;
        }
    }

    off_t foff = lseek(file->fd, file->pos, SEEK_SET);
    if (foff < 0) {
        perror("lseek");
        return -1;
    }

    len = MIN(len, file->sz - foff);
    file->pos += len;

    return writen(file->fd, data, len);
}

int
filevec_write(struct bt_file *v, size_t n, const void *data, size_t len)
{
    while (len) {
        const ssize_t nxfer = file_write(v, data, len);
        if (nxfer < 0) {
            return -1;
        }

        if (nxfer == 0) {
            v = v + 1;
            if (!--n) {
                break;
            }
            file_set_pos(v, 0);
        }

        data = (byte *)data + nxfer;
        len -= nxfer;
    }

    return 0;
}

int
bt_disk_write(BT_DiskMgr *m, const void *data, uint64 base, size_t len)
{
    assert (data != NULL);

    assert (m != NULL);

    struct bt_file *file = file_from_offset(&m->files, base);
    if (!file) {
        return -1;
    }

    file_set_pos(file, base - file->off);

    const size_t remafter = m->files.n - (file - &m->files.data[0]);
    return filevec_write(file, remafter, data, len);
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
