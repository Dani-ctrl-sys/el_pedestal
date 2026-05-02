/*
 * test_squeeze_bug.c
 *
 * Test de diagnóstico para el bug en keccak_squeezeblocks.
 *
 * HIPÓTESIS:
 *   Si llamamos shake128_squeezeblocks(buf, 1) dos veces seguidas sobre el
 *   mismo estado, ambas llamadas producirán bloques IDÉNTICOS porque el
 *   estado no avanza entre llamadas (la permutación se omite).
 *
 * PROTOCOLO:
 *   Experimento A — Dos llamadas sucesivas de 1 bloque:
 *     absorb(seed) → squeezeblocks(buf1, 1) → squeezeblocks(buf2, 1)
 *     Si BUG: buf1 == buf2
 *
 *   Experimento B — Una sola llamada de 2 bloques (referencia correcta):
 *     absorb(seed) → squeezeblocks(ref, 2)
 *     ref[0..167]   = primer bloque correcto
 *     ref[168..335] = segundo bloque correcto
 *
 *   Veredicto: comparar buf1 vs ref[0..167]   → deben ser IGUALES
 *              comparar buf2 vs ref[168..335]  → deben ser IGUALES si NO hay bug
 *                                              → serán DISTINTOS si hay bug
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "fips202.h"

#define RATE SHAKE128_RATE  /* 168 bytes */

int main(void) {
    uint8_t seed[32];
    for (int i = 0; i < 32; i++) seed[i] = (uint8_t)i;

    /* ------------------------------------------------------------------ */
    /* Experimento A: dos llamadas sucesivas de 1 bloque                  */
    /* ------------------------------------------------------------------ */
    keccak_state stateA;
    uint8_t buf1[RATE];
    uint8_t buf2[RATE];

    shake128_absorb(&stateA, seed, 32);
    shake128_squeezeblocks(buf1, 1, &stateA);   /* bloque 1 */
    shake128_squeezeblocks(buf2, 1, &stateA);   /* bloque 2 (¿o bloque 1 de nuevo?) */

    /* ------------------------------------------------------------------ */
    /* Experimento B: una sola llamada de 2 bloques (referencia correcta) */
    /* ------------------------------------------------------------------ */
    keccak_state stateB;
    uint8_t ref[RATE * 2];

    shake128_absorb(&stateB, seed, 32);
    shake128_squeezeblocks(ref, 2, &stateB);    /* 2 bloques de golpe */

    /* ------------------------------------------------------------------ */
    /* Análisis de resultados                                              */
    /* ------------------------------------------------------------------ */
    printf("=== DIAGNÓSTICO: keccak_squeezeblocks ===\n\n");

    /* Comprobación 1: buf1 vs ref[0..167] — deben ser idénticos */
    int c1_ok = (memcmp(buf1, ref, RATE) == 0);
    printf("[CHECK 1] buf1 == ref[bloque_1]: %s\n", c1_ok ? "OK" : "FAIL");
    if (!c1_ok) {
        printf("  ERROR: El primer squeeze no produce el bloque correcto.\n");
    }

    /* Comprobación 2: buf2 vs ref[168..335] — son distintos si hay bug */
    int c2_ok = (memcmp(buf2, ref + RATE, RATE) == 0);
    printf("[CHECK 2] buf2 == ref[bloque_2]: %s\n", c2_ok ? "OK" : "FALLO DE BUG");
    if (!c2_ok) {
        printf("  --> buf2 NO coincide con el segundo bloque de referencia.\n");
    }

    /* Comprobación 3: ¿son buf1 y buf2 idénticos? (predicción del bug) */
    int duplicated = (memcmp(buf1, buf2, RATE) == 0);
    printf("[CHECK 3] buf1 == buf2 (duplicado): %s\n", duplicated ? "SI — BUG CONFIRMADO" : "NO — estado avanza correctamente");

    /* Primeros bytes para inspección visual */
    printf("\nPrimeros 8 bytes de cada buffer:\n");
    printf("  buf1 : ");
    for (int i = 0; i < 8; i++) printf("%02x ", buf1[i]);
    printf("\n");
    printf("  buf2 : ");
    for (int i = 0; i < 8; i++) printf("%02x ", buf2[i]);
    printf("\n");
    printf("  ref+0: ");
    for (int i = 0; i < 8; i++) printf("%02x ", ref[i]);
    printf("\n");
    printf("  ref+R: ");
    for (int i = 0; i < 8; i++) printf("%02x ", ref[RATE + i]);
    printf("\n");

    /* Veredicto final */
    printf("\n=== VEREDICTO ===\n");
    if (duplicated) {
        printf("[BUG ACTIVO] Las dos llamadas sucesivas producen el mismo bloque.\n");
        printf("             El estado no avanza entre squeeze calls.\n");
        return 1;
    } else if (c1_ok && c2_ok) {
        printf("[SIN BUG]   Ambas llamadas producen bloques distintos y correctos.\n");
        return 0;
    } else {
        printf("[ERROR INESPERADO] Estado inconsistente. Revisar la implementación.\n");
        return 2;
    }
}
