#ifndef POLY_H
#define POLY_H

#include <stdint.h>
#include "arithmetic.h"

#define N 256

/* === Selección de nivel de seguridad (compilación) === */
#ifndef DILITHIUM_MODE
#define DILITHIUM_MODE 2
#endif

#if DILITHIUM_MODE == 2
#define K 4
#define L 4
#define ETA 2
#define TAU 39
#define BETA 78
#define GAMMA1 (1 << 17)
#define GAMMA2 ((Q - 1) / 88)
#define OMEGA 80
#elif DILITHIUM_MODE == 3
#define K 6
#define L 5
#define ETA 4
#define TAU 49
#define BETA 196
#define GAMMA1 (1 << 19)
#define GAMMA2 ((Q - 1) / 32)
#define OMEGA 55
#elif DILITHIUM_MODE == 5
#define K 8
#define L 7
#define ETA 2
#define TAU 60
#define BETA 120
#define GAMMA1 (1 << 19)
#define GAMMA2 ((Q - 1) / 32)
#define OMEGA 75
#else
#error "DILITHIUM_MODE debe ser 2, 3, o 5"
#endif

#define D  13
#define M  ((Q - 1) / (2 * GAMMA2))

/* === Tipos === */
typedef struct { int32_t coeffs[N]; } poly;
typedef struct { poly vec[L]; }       polyvecl;
typedef struct { poly vec[K]; }       polyveck;

/* === Aritmética de polinomios === */
void poly_add(poly *r, const poly *a, const poly *b);
void poly_sub(poly *r, const poly *a, const poly *b);
void poly_reduce(poly *a);
void poly_caddq(poly *a);
void poly_uniform(poly *a, const uint8_t seed[32], uint16_t nonce);

/* === Descomposición y compresión === */
void power2round(int32_t *r1, int32_t *r0, int32_t a);
void decompose(int32_t *r1, int32_t *r0, int32_t a);
int32_t highbits(int32_t a);
int32_t lowbits(int32_t a);

void poly_power2round(poly *r1, poly *r0, const poly *a);
void poly_decompose(poly *r1, poly *r0, const poly *a);
void poly_highbits(poly *r1, const poly *a);
void poly_lowbits(poly *r0, const poly *a);

/* === Hints === */
int32_t make_hint(int32_t z, int32_t r);
int32_t use_hint(int32_t h, int32_t r);
unsigned int poly_make_hint(poly *h, const poly *z, const poly *r);
void poly_use_hint(poly *r1, const poly *h, const poly *r);

/* === Vectores de polinomios === */
void polyvecl_ntt(polyvecl *v);
void polyvecl_invntt(polyvecl *v);
void polyveck_ntt(polyveck *v);
void polyveck_invntt(polyveck *v);
void polyvecl_pointwise_acc(poly *r, const poly *a, const polyvecl *b);

void polyveck_add(polyveck *r, const polyveck *a, const polyveck *b);
void polyveck_sub(polyveck *r, const polyveck *a, const polyveck *b);
void polyveck_reduce(polyveck *v);
void polyveck_caddq(polyveck *v);
void polyveck_power2round(polyveck *r1, polyveck *r0, const polyveck *v);
void polyveck_decompose(polyveck *r1, polyveck *r0, const polyveck *v);
unsigned int polyveck_make_hint(polyveck *h, const polyveck *z, const polyveck *r);
void polyveck_use_hint(polyveck *r1, const polyveck *h, const polyveck *r);

#endif /* POLY_H */
