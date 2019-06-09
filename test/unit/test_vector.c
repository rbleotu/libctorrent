#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "util/common.h"
#include "util/vector.h"

#if !defined(N)
#  define N ((size_t)1E2)
#endif

VECTOR_DEFINE(int, int);
VECTOR_DEFINE(char *, string);

static void
test1(void)
{
    Vector(int) v = VECTOR();

    for (size_t i=0; i<N; i++) {
        vector_pushback(&v, i);
    }

    VECTOR_FOREACH(&v, x, int) {
        printf("%d\n", *x);
    }

    vector_destroy(&v);
}

static void
randstr(char dest[], size_t len)
{
    static const char alpha[] = "abcdefghijklmnopqrstuvwxyz";
    while (len--)
        *dest++ = alpha[rand() % (sizeof(alpha) - 1)];
    *dest++ = '\0';
}

static void
test2(void)
{
    Vector(string) v = VECTOR();
    char buf[32];

    for (size_t i=0; i<N; i++) {
        randstr(buf, 8);
        char * const tmp = strdup(buf);
        vector_pushback(&v, tmp);
    }

    VECTOR_FOREACH(&v, x, char *) {
        puts(*x);
        free(*x);
        *x = NULL;
    }

    vector_destroy(&v);
}

int main(int argc, char *argv[])
{
    test1();
    test2();
    return 0;
}
