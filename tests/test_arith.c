#include <stdio.h>
#include "arithmetic.h"

int main(void) {
    // Caso 1: a < Q (no debe reducir)
    int32_t a1 = 5000;
    int32_t r1 = conditional_subq(a1);
    
    // Caso 2: a >= Q (debe reducir: 9000000 - 8380417 = 619583)
    int32_t a2 = 9000000;
    int32_t r2 = conditional_subq(a2);

    // Caso 3: Resta que da negativo (debe sumar Q: -5000 + 8380417 = 8375417)
    int32_t a3 = -5000;
    int32_t r3 = caddq(a3);
    
    // Caso 4: Resta que da positivo (debe quedar igual)
    int32_t a4 = 5000;
    int32_t r4 = caddq(a4);

    printf("Test 1 - Entrada: %d | Salida: %d (Esperado: 5000)\n", a1, r1);
    printf("Test 2 - Entrada: %d | Salida: %d (Esperado: 619583)\n", a2, r2);
    printf("Test 3 - Entrada: %d | Salida: %d (Esperado: 8375417)\n", a3, r3);
    printf("Test 4 - Entrada: %d  | Salida: %d (Esperado: 5000)\n", a4, r4);

    if (r1 == 5000 && r2 == 619583) {
        printf("\n✅ RESULTADO: El pedestal es estable. Aritmética modular correcta.\n");
    } else {
        printf("\n❌ RESULTADO: Error en la reducción. Revisa las máscaras.\n");
    }

    return 0;
}