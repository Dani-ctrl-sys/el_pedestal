#include "arithmetic.h"

int32_t conditional_subq(int32_t a) {
    int32_t res = a - Q;
    int32_t mask = res >> 31;
    
    // Compensación condicional empleando proyección de máscara por underflow.
    return res + (Q & mask);
}

/*
 * Suma condicional (constant-time) para magnitudes moderadas.
 * Precondición: 'a' acotado temporalmente en -Q < a < Q.
 */
int32_t caddq(int32_t a) {
    // Evaluación branchless vía extensión de signo y operación bitwise.
    int32_t mask = a >> 31;
    return a + (Q & mask);
}

int32_t montgomery_reduce(int64_t a) {
    // Fase 1: Multiplicación truncada con evaluación modular QINV.
    int32_t t = (int32_t)a * QINV;
    
    // Fase 2-4: Conversión expansiva por desplazamiento logarítmico para absorción y normalización.
    int32_t res = (int32_t)((a - (int64_t)t * Q) >> 32);
    
    return res;
}

int32_t barrett_reduce(int32_t a) {
    // Paso 1: Estimación matemática del cociente optimizando redondeo para corrección vectorial.
    int32_t t = (int32_t)(((int64_t)a * BARRETT_MULTIPLIER + (1 << 25)) >> 26);
    
    // Paso 2: Corrección residual para alcanzar ajuste escalar.
    int32_t res = a - t * Q;
    
    return res;
}