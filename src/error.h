#ifndef ERROR_H_
#define ERROR_H_

enum {
    BT_ENONE,
    BT_EREAD,
    BT_EALLOC,
    BT_ESYNTAX,
    BT_EEOF,
    BT_EOVERFLOW,
    BT_EFORMAT,
    BT_EMKDIR,
    BT_EDIRALIAS,
    BT_EOPEN,
    BT_ENOTRACKER,
    BT_EBADPROTO,
    BT_ETRACKERURL,
    BT_EGETADDR,
    BT_EUDPSEND,
    BT_EBADRESPONSE,
    BT_ETIMEOUT,
    BT_ESIGACTION,
    BT_ELSEEK,
    BT_ENOPEERS,
    BT_ENETRECV,
    BT_ENETSEND,

    BT_ERROR_CNT
};

extern int bt_errno;

extern const char *
bt_strerror(void);

#endif
