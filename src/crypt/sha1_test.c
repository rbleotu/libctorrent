#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <errno.h>
#include <string.h>

#include "sha1.h"

static uint8_t *
file_get_contents(FILE *f, size_t *len)
{
#define INIT_SZ (1 << 10)
    uint8_t *res = malloc(INIT_SZ);
    if (!res)
        return NULL;
    size_t i = 0, sz = INIT_SZ;
    size_t xfer = 0;
    while ((xfer = fread(res + i, 1, sz - i, f))) {
        if ((i + xfer) >= sz) {
            uint8_t *tmp = realloc(res, sz <<= 1);
            if (!tmp) {
                perror("realloc");
                break;
            }
            res = tmp;
        }
        i += xfer;
    }
    res[i] = '\0';
    *len = i;
    return realloc(res, i + 1);
}

int
main(int argc, char *argv[])
{
    size_t len = 0;
    uint8_t digest[SHA1_DIGEST_LEN];
    uint8_t *data;
    FILE *f = freopen(argv[1], "rb", stdin);
    if (!f) {
        fprintf(stderr, "Failed to open '%s': %s\n", argv[1], strerror(errno));
        return 1;
    }

    data = file_get_contents(f, &len);
    if (!data)
        return 1;

    t_sha1(digest, data, len);
    for (size_t i = 0; i < sizeof(digest); i++)
        printf("%02x", (int)digest[i]);
    putchar('\n');

    return fclose(f);
}
