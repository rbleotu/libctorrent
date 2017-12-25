#ifndef BENCODING_H_
#define BENCODING_H_

typedef struct t_bcode T_BCode;

typedef int64_t T_BInt;
typedef struct t_bstring *T_BString;
typedef struct t_barray *T_BList;
typedef struct t_barray *T_BDict;
typedef struct t_barray *T_BArray;

enum BCODE_TYPE {
    BCODE_NONE,
    BCODE_INT,
    BCODE_STRING,
    BCODE_LIST,
    BCODE_DICT,

    BCODE_TYPE_CNT
};

#define B_ISNIL(b) ((b).type == BCODE_NONE)
#define B_ISLST(b) ((b).type == BCODE_LIST)
#define B_ISDICT(b) ((b).type == BCODE_DICT)

#define B_NUM(x) ((x).u.num)
#define B_STR(x) ((x).u.str)
#define B_LST(x) ((x).u.lst)
#define B_DICT(x) ((x).u.dict)

struct t_bcode {
    int type;
    union {
        T_BInt num;
        T_BString str;
        T_BList lst;
        T_BDict dict;
    } u;
};

extern int
t_bdecode_file(FILE *f, T_BCode *res);

extern int
t_bdecode_str(const uint8_t str[], size_t len, T_BCode *res);

extern void
t_bcode_fprint(FILE *out, T_BCode b);

#endif
