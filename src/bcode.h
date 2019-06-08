#ifndef BENCODING_H_
#define BENCODING_H_

#include <stdint.h>
#include "util/common.h"

enum {
    BCODE_NONE,
    BCODE_INT,
    BCODE_STRING,
    BCODE_LIST,
    BCODE_DICT,

    BCODE_TYPE_CNT
};

typedef struct bcode BT_BCode;

typedef struct barray *BArray;

typedef int64_t BT_BInt;
typedef struct barray *BT_BList;
typedef struct barray *BT_BDict;
typedef struct bstring *BT_BString;

#define b_isnil(b) ((b).id == BCODE_NONE)
#define b_isnum(b) ((b).id == BCODE_INT)
#define b_isstring(b) ((b).id == BCODE_STRING)
#define b_islist(b) ((b).id == BCODE_LIST)
#define b_isdict(b) ((b).id == BCODE_DICT)

#define b_num(x) ((x).u.num)
#define b_string(x) ((x).u.string->content)
#define b_list(x) ((x).u.list)
#define b_dict(x) ((x).u.dict)

#define b_strlen(x) ((x).u.string->len)


struct bstring {
    size_t len;
    uint8_t content[];
};

struct bcode {
    int id;

    union {
        BT_BInt num;
        BT_BString string;
        BT_BList list;
        BT_BDict dict;
    } u;
};

struct barray {
    size_t n, cap;
    BT_BCode base[];
};

int
bt_bdecode_file(OUT struct bcode *b, IN FILE *f);

int
bt_bdecode_str(OUT struct bcode *b, IN const uint8 *str, IN size_t len);

size_t
bt_bencode(OUT uint8 str[], IN size_t len, IN struct bcode b);

void
bt_bcode_fprint(IN FILE *out, IN struct bcode b);

void
bt_bcode_free(BT_BCode b);

BT_BCode
bt_bcode_get(IN BT_BCode b, ...);

#endif
