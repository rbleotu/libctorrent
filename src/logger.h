#ifndef LOGGER_H_
#define LOGGER_H_

enum {
    BT_LOG_INFO,
    BT_LOG_WARN,
    BT_LOG_ERROR,
};

struct bt_logger {
    void (*log)(int type, const char *fmt, ...);
};

typedef struct bt_logger BT_Logger;

#endif
