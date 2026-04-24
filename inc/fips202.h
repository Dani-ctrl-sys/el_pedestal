#ifndef FIPS202_H
#define FIPS202_H

#include <stddef.h>
#include <stdint.h>

/* Tasas (Rates) de Inserción para la Criptografía Keccak 
 * 168 bytes por vuelta para SHAKE-128 = ~128 bits de seguridad en colisiones
 * 136 bytes por vuelta para SHAKE-256 = ~256 bits de seguridad
 */
#define SHAKE128_RATE 168
#define SHAKE256_RATE 136

/*
 * Estructura para almacenar el estado continuo de Keccak-f[1600].
 * - s: Es la matriz cúbica interna equivalente a 25 palabras de 64 bits (200 bytes).
 * - pos: Mantendrá nuestro contador de cuántos bytes llevamos procesados en la vuelta actual.
 */
typedef struct {
  uint64_t s[25];  
  unsigned int pos;
} keccak_state;

/* ========================================================================= */
/* API PARA SHAKE-128                                                        */
/* ========================================================================= */

void shake128_absorb(keccak_state *state, const uint8_t *in, size_t inlen);
void shake128_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state);
void shake128(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);

/* ========================================================================= */
/* API PARA SHAKE-256                                                        */
/* ========================================================================= */

void shake256_absorb(keccak_state *state, const uint8_t *in, size_t inlen);
void shake256_squeezeblocks(uint8_t *out, size_t nblocks, keccak_state *state);
void shake256(uint8_t *out, size_t outlen, const uint8_t *in, size_t inlen);


#endif // FIPS202_H
