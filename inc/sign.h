#ifndef SIGN_H
#define SIGN_H

#include <stdint.h>
#include "poly.h"
#include "packing.h"

/* Tamaños de FIPS 204 para DILITHIUM_MODE 2 */
#define CRYPTO_SECRETKEYBYTES 2560
#define CRYPTO_BYTES          2420

int crypto_sign_keypair(uint8_t *pk, uint8_t *sk);
int crypto_sign(uint8_t *sig, size_t *siglen, const uint8_t *m, size_t mlen, const uint8_t *sk);

#endif
