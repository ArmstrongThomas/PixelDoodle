#ifndef SEED_STORE_H
#define SEED_STORE_H
#include <stddef.h>
#include <stdbool.h>
#define SEED_BYTES 64
/* The directory must already exist. A pending claim blocks all later attempts
 * until the Windows deployment helper provisions fresh randomness. */
bool seed_claim(const char *dir, unsigned char seed[SEED_BYTES]);
bool seed_commit(const char *dir, const unsigned char next[SEED_BYTES]);
#endif
