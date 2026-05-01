#include <stdio.h>
#include <stdint.h>
#include "poly.h"
#include "fips202.h"

int main() {
    poly a;
    uint8_t seed[32];
    uint16_t nonce = 0x1234;
    int i, error_count = 0;

    // 1. Llenamos la semilla con algo fijo (ej: 0, 1, 2...)
    for(i = 0; i < 32; i++) seed[i] = i;

    printf("--- TEST: poly_uniform ---\n");
    printf("Generando polinomio con nonce: 0x%04X\n\n", nonce);

    // 2. Ejecutamos tu funcion
    poly_uniform(&a, seed, nonce);

    // 3. Imprimimos una muestra para ver el \"caos\"
    printf("Muestra de los primeros 8 coeficientes:\n");
    for(i = 0; i < 8; i++) {
        printf("  a[%d] = %d\n", i, a.coeffs[i]);
    }

    // 4. Validacion automatica contra Q
    for(i = 0; i < N; i++) {
        if(a.coeffs[i] >= Q) {
            printf("\nERROR! Coeficiente %d fuera de rango: %d (Q=%d)\n", i, (int)a.coeffs[i], Q);
            error_count++;
        }
    }

    if(error_count == 0) {
        printf("\n[OK] TEST PASADO: Los 256 coeficientes son menores que Q.\n");
    } else {
        printf("\n[FAIL] TEST FALLIDO: Se encontraron %d errores.\n", error_count);
    }

    return 0;
}
