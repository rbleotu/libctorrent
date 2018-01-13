#ifndef SHA1_H_
#define SHA1_H_

#define SHA1_DIGEST_LEN 20

#define check_hash(a, b) (!memcmp((a), (b), SHA1_DIGEST_LEN))

void
bt_sha1(uint8_t digest[], const uint8_t msg[], size_t len);

#endif
