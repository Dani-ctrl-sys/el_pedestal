#ifndef PACKING_H
#define PACKING_H

#include <stdint.h>
#include "poly.h"

/* Definiciones de tamaños de serialización (FIPS 204) */
#define POLYT1_PACKEDBYTES  320
#define POLYETA_PACKEDBYTES 96
#define POLYZ_PACKEDBYTES   576
#define POLYT0_PACKEDBYTES  416
#define CRYPTO_PUBLICKEYBYTES (32 + K * POLYT1_PACKEDBYTES)

/**
 * @brief Serialización de polinomios y estructuras de datos para ML-DSA.
 */

void pack_pk(uint8_t pk[CRYPTO_PUBLICKEYBYTES], const uint8_t rho[32], const polyveck *t1);
void unpack_pk(uint8_t rho[32], polyveck *t1, const uint8_t pk[CRYPTO_PUBLICKEYBYTES]);

void polyt1_pack(uint8_t *r, const poly *a);
void polyt1_unpack(poly *r, const uint8_t *a);

void polyeta_pack(uint8_t *r, const poly *a);
void polyeta_unpack(poly *r, const uint8_t *a);

void polyt0_pack(uint8_t *r, const poly *a);
void polyt0_unpack(poly *r, const uint8_t *a);

void pack_sk(uint8_t sk[CRYPTO_SECRETKEYBYTES], 
             const uint8_t rho[32], const uint8_t K_key[32], const uint8_t tr[64],
             const polyvecl *s1, const polyveck *s2, const polyveck *t0);

void unpack_sk(uint8_t rho[32], uint8_t K_key[32], uint8_t tr[64], 
               polyvecl *s1, polyveck *s2, polyveck *t0, 
               const uint8_t *sk);

void polyz_pack(uint8_t *r, const poly *a);
void polyveck_pack_w1(uint8_t *r, const polyveck *v);
void pack_sig(uint8_t *sig, const uint8_t c[32], const polyvecl *z, const polyveck *h);

#endif /* PACKING_H */
