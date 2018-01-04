#ifndef BENCODING_H_
#define BENCODING_H_

#include <stdint.h>
#include "../common.h"

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

#define B_ISNIL(b) ((b).id == BCODE_NONE)
#define B_ISNUM(b) ((b).id == BCODE_INT)
#define B_ISSTRING(b) ((b).id == BCODE_STRING)
#define B_ISLIST(b) ((b).id == BCODE_LIST)
#define B_ISDICT(b) ((b).id == BCODE_DICT)

#define B_NUM(x) ((x).u.num)
#define B_STRING(x) ((x).u.string)
#define B_LIST(x) ((x).u.list)
#define B_DICT(x) ((x).u.dict)


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
bt_bdecode_str(OUT struct bcode *b, IN const u8 *str, IN size_t len);

size_t
bt_bencode(OUT u8 str[], IN size_t len, IN struct bcode b);

void
bt_bcode_fprint(IN FILE *out, IN struct bcode b);

BT_BCode
bt_bcode_get(IN BT_BCode b, ...);

#endif
