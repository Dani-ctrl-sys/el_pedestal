#include "arithmetic.h"

int32_t conditional_subq(int32_t a) {
    int32_t res = a - Q;
    int32_t mask = res >> 31;
    
    // Si res < 0 (máscara de unos), sumamos Q de vuelta.
    // Si res >= 0 (máscara de ceros), sumamos 0.
    return res + (Q & mask);
}

/*
 * Asume: a es el resultado de una resta, por lo que -Q < a < Q
 * Devuelve: a + Q si a < 0, o 'a' si a >= 0.
 * Restricción: Tiempo constante estricto.
 */
int32_t caddq(int32_t a) {
    // 1. Extrae el bit de signo de 'a' para crear la máscara.
    // 2. Aplica lógica bit a bit para sumar Q solo cuando la máscara sea de unos.
    
    // Tu lógica branchless aquí:
    int32_t mask = a >> 31;
    return a + (Q & mask);
}