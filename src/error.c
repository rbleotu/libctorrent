#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#include "error.h"

static const char *const msg_tab[] = {
        [BT_ENONE] = "success",
        [BT_EREAD] = "error reading from file",
        [BT_EALLOC] = "failed to allocate memory",
        [BT_ESYNTAX] = "syntax error in bcode",
        [BT_EOVERFLOW] = "overflow occuring when reading number",
        [BT_EEOF] = "unexpected end of file",
        [BT_EFORMAT] = "bad format, not a torrent file",
        [BT_EMKDIR] = "failed to create directory",
        [BT_EDIRALIAS] = "file is aliasing directory",
        [BT_EOPEN] = "error creating file",
        [BT_ENOTRACKER] = "torrent file has no trackers",
        [BT_EBADPROTO] = "bad tracker protocol",
        [BT_ETRACKERURL] = "bad tracker url",
        [BT_EGETADDR] = "failed to resolve tracker address",
        [BT_EUDPSEND] = "sendto() failed",
        [BT_EBADRESPONSE] = "bad tracker response",
        [BT_ETIMEOUT] = "timed out waiting for tracker",
        [BT_ESIGACTION] = "sigaction failed",
        [BT_ELSEEK] = "lseek() failed",
        [BT_ENOPEERS] = "torrent has no peers"


};


int bt_errno;

extern const char *
bt_strerror(void)
{
    return msg_tab[bt_errno];
}
