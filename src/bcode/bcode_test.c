#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include "bencoding.h"

int
main(int argc, char *argv[])
{
    const char *fname = NULL;
    const char *expr = NULL;
    T_BCode b;

    for (char *arg; arg = *++argv, --argc;) {
        if (*arg == '-') {
            switch (*++arg) {
            case 'e': {
                expr = arg[1] ? arg + 1 : (--argc, *++argv);
                if (!expr) {
                    fputs("expected expression for option 'e'\n", stderr);
                    return 1;
                }
            } break;
            default:
                fprintf(stderr, "'%s' option not recognized\n", arg);
                return 1;
            }
        } else {
            if (fname) {
                fputs("at most 1 file arg allowed\n", stderr);
                return 1;
            }
            fname = arg;
        }
    }

    if (expr) {
        size_t len = strlen(expr);
        if (t_bdecode_str((const uint8_t *)expr, len, &b) < 0) {
            fputs(t_bcode_errstr, stderr);
            return 1;
        }
    } else if (fname) {
        FILE *f = fopen(fname, "rb");
        if (!f) {
            fprintf(stderr, "Failed to open '%s': %s\n", fname,
                    strerror(errno));
            return 1;
        }
        if (t_bdecode_file(f, &b) < 0) {
            fputs(t_bcode_errstr, stderr);
            return 1;
        }
    } else {
        fprintf(stderr, "Usage:%s [-e<expression>|<filename>]\n", argv[0]);
        return 1;
    }

    t_bcode_fprint(stdout, b);
    putchar('\n');

    if (!B_ISDICT(b)) {
        fputs("not valid torrent bencoding\n", stderr);
        return 1;
    }

    // const char *str = "announce-list";
    // T_BCode lst = t_bcode_get(t_bcode_get(b, str, strlen(str)), 0);
    // for(int i=0; i<lst.lst->n; i++){
    //    t_bcode_fprint(stdout, t_bcode_get(lst,i));
    //    putchar('\n');
    //}

    return 0;
}
