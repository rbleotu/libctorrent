#include <unistd.h>

#include <assert.h>
#include "thread_msg.h"

int
bt_thmsg_send(int pfd, struct bt_thmsg m)
{
    return write(pfd, &m, sizeof(m));
}

int
bt_thmsg_recv(int pfd, struct bt_thmsg *m)
{
    return read(pfd, m, sizeof(*m));
}
