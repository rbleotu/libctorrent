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

local pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;

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

local size_t
file_from_offset(struct bt_file v[], size_t n, off_t x)
{
    size_t i = 0;
    while (++i < n)
        if (v[i].off >= x)
            break;
    return i - 1;
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
bt_disk_read_piece(IN BT_DiskMgr m, uint8 data[], size_t len, off_t off)
{
    assert(m && data);

    size_t i = file_from_offset(m->files, m->nfiles, off);
    ssize_t nread, nxfer;

    off -= m->files[i].off;

    for (; len; off = 0, i++) {
        assert(i < m->nfiles);

        if (lseek(m->files[i].fd, off, SEEK_SET) == -1) {
            bt_errno = BT_ELSEEK;
            return -1;
        }

        size_t fsz = m->files[i].sz - off;
        nread = MIN(fsz, len);
        if ((nxfer = readn(m->files[i].fd, data, nread)) < 0) {
            bt_errno = BT_EREAD;
            return -1;
        }
        if (nxfer < nread) {
            memset(data + nxfer, 0, nread - nxfer);
            nread = len;
        }

        len -= nread, data += nread;
    }

    return 0;
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

int
bt_disk_write_piece(IN BT_DiskMgr m, uint8 data[], size_t len, off_t off)
{
    assert(m && data);

    size_t i = file_from_offset(m->files, m->nfiles, off);
    ssize_t nwrite, nxfer;
    off -= m->files[i].off;

    pthread_mutex_lock(&lock);
    for (; len; off = 0, i++) {
        assert(i < m->nfiles);

        if (lseek(m->files[i].fd, off, SEEK_SET) == -1) {
            bt_errno = BT_ELSEEK;
            return -1;
        }
        size_t fsz = m->files[i].sz - off;
        nwrite = MIN(fsz, len);
        if ((nxfer = writen(m->files[i].fd, data, nwrite)) < nwrite) {
            bt_errno = BT_EREAD;
            return -1;
        }

        off = 0;
        len -= nwrite, data += nwrite;
    }
    pthread_mutex_unlock(&lock);

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
