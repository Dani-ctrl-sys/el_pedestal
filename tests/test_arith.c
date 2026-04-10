#include <stdio.h>
#include "arithmetic.h"
#include <stdint.h>
#include "ntt.h"

int test_ntt_integridad(void);

int main(void) {
    // Prueba 1: Valor validado sin requisito de reducción (a < Q).
    int32_t a1 = 5000;
    int32_t r1 = conditional_subq(a1);
    
    // Prueba 2: Exceso de módulo provocando reducción compensatoria por Q.
    int32_t a2 = 9000000;
    int32_t r2 = conditional_subq(a2);

    // Prueba 3: Ajuste modular de diferencia por desbordamiento negativo.
    int32_t a3 = -5000;
    int32_t r3 = caddq(a3);
    
    // Prueba 4: Estabilización matemática para diferencial positivo inalterado.
    int32_t a4 = 5000;
    int32_t r4 = caddq(a4);

    // Prueba 5: Ensayo de reducción de Montgomery posterior a un producto en 64 bits.
    int32_t coef1 = 8000000;
    int32_t coef2 = 8000000;

    // Prueba 6: Ensayo estricto limitando simetría asintótica para reducción de Barrett.
    // Confirmación bajo condiciones numéricas en margen positivo.
    int32_t a6_pos = 15000000;
    int32_t r6_pos = barrett_reduce(a6_pos);

    // Confirmación de la simetría paramétrica equivalente bajo dominio negativo.
    int32_t a6_neg = -15000000;
    int32_t r6_neg = barrett_reduce(a6_neg);
    
    // Simulación contextual paralela previa al procesamiento NTT: límite técnico a 64 bits.
    int64_t producto_gigante = (int64_t)coef1 * coef2; 
    
    // Proceso compensatorio de condensación al espectro computacional natural (32 bits).
    int32_t r5 = montgomery_reduce(producto_gigante);

    printf("Test 1 - Entrada: %d | Salida: %d (Esperado: 5000)\n", a1, r1);
    printf("Test 2 - Entrada: %d | Salida: %d (Esperado: 619583)\n", a2, r2);
    printf("Test 3 - Entrada: %d | Salida: %d (Esperado: 8375417)\n", a3, r3);
    printf("Test 4 - Entrada: %d  | Salida: %d (Esperado: 5000)\n", a4, r4);
    printf("Test 5 - Montgomery | Salida: %d\n", r5);
    printf("Test 6A - Barrett Positivo | Salida: %d (Esperado: -1760834)\n", r6_pos);
    printf("Test 6B - Barrett Negativo | Salida: %d (Esperado: 1760834)\n", r6_neg);

    if (r1 == 5000 && r2 == 619583) {
        printf("\n✅ RESULTADO: El pedestal es estable. Aritmética modular correcta.\n");
    } else {
        printf("\n❌ RESULTADO: Error en la reducción. Revisa las máscaras.\n");
    }
    if (r1 == 5000 && r2 == 619583 && r6_pos == -1760834 && r6_neg == 1760834) {
        printf("\n✅ RESULTADO: El pedestal es estable. Fase 1 superada.\n");
    } else {
        printf("\n❌ RESULTADO: Error en la aritmética.\n");
    }

    printf("\n--- Ejecutando pruebas de Fase 2 (NTT) ---\n");
    test_ntt_integridad();

    return 0;
}

int test_ntt_integridad(void) {
    int32_t a[256];
    int32_t a_orig[256];
    int i;
    int errores = 0;

    int32_t b[256];

    // 1. Inyección de datos controlados
    for(i = 0; i < 256; i++) {
        a[i] = i;          // Patrón de prueba lineal simple
        a_orig[i] = a[i];  // Copia de seguridad intacta
        b[i] = 1;          // Polinomio identidad (unos)
    }

    // 2. Ida: Entrar al dominio NTT
    poly_ntt(a);

    // 3. Multiplicación en dominio NTT (Aplica el déficit de R^-1 requerido para la INTT)
    poly_mul_pointwise(a, a, b);

    // 4. Vuelta: Salir del dominio NTT
    poly_invntt(a);

    // 4. Auditoría de memoria
    for(i = 0; i < 256; i++) {
        // La resta modular puede dejar el valor desplazado por un múltiplo de Q.
        // En un entorno riguroso, comprobamos la congruencia, pero para números 
        // pequeños iniciales, la recuperación debería ser exacta.
        if(a[i] != a_orig[i]) {
            printf("[ERROR] Indice %d corrupto. Esperado: %d, Obtenido: %d\n", i, a_orig[i], a[i]);
            errores++;
        }
    }

    if(errores == 0) {
        printf("[EXITO] La cadena NTT -> INTT es matematicamente hermetica.\n");
        return 0; // Pasa el test
    } else {
        printf("[FALLO] Se detectaron %d errores de reconstruccion.\n", errores);
        return -1; // Falla el test
    }
}