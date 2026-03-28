#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include <stdint.h>

/* Parámetros estáticos del cuerpo paramétrico finito */
#define Q 8380417
#define QINV 58728449
#define BARRETT_MULTIPLIER 8

/* Operadores modulares en tiempo estructurado estricto (constant-time) */
int32_t conditional_subq(int32_t a);
int32_t caddq(int32_t a);

/* Técnicas paramétricas de reducción asintótica y acoplamiento */
int32_t montgomery_reduce(int64_t a);
int32_t barrett_reduce(int32_t a);

#endif