#ifndef NTT_H
#define NTT_H

#include <stdint.h>

// Expone la tabla precalculada de factores de torsión en dominio Montgomery
extern const int32_t zetas[256];
void poly_ntt(int32_t a[256]);

#endif