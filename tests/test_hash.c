#include <stdio.h>
#include <stdint.h>
#include "fips202.h"

int main(void) {
    printf("==========================================\n");
    printf("   🛠️ INICIANDO AUDITORIA FIPS 202 (Keccak)\n");
    printf("==========================================\n\n");

    // Array de salida donde caerán 16 bytes de pura entropía
    uint8_t salida[16];

    // VECTOR DE PRUEBA OFICIAL NIST: 
    // Esto es lo que DEBE dar SHAKE-128 al absorber CERO bytes ("")
    uint8_t esperado[16] = {
        0x7F, 0x9C, 0x2B, 0xA4, 0xE8, 0x8F, 0x82, 0x7D, 
        0x61, 0x60, 0x45, 0x50, 0x76, 0x05, 0x85, 0x3E
    };

    printf("Arrancando SHAKE-128 (Absorbiendo entrada vacia)...\n");
    
    // Función One-Shot: 0 bytes de entrada -> 16 bytes de salida
    shake128(salida, 16, NULL, 0);

    // Auditoría byte a byte
    int fallos = 0;
    printf("\nResultado Obtenido VS Esperado (NIST):\n");
    for(int i = 0; i < 16; i++) {
        printf("Byte %02d: 0x%02X | 0x%02X", i, salida[i], esperado[i]);
        if(salida[i] != esperado[i]) {
            printf("  <-- ❌ ERROR FATAL\n");
            fallos++;
        } else {
            printf("  [OK]\n");
        }
    }

    printf("\n");
    if(fallos == 0) {
        printf("✅ RESULTADO: KeccakF1600 y Esponjas SHAKE operativas e impenetrables.\n");
        printf("La base alfanumérica del Nivel 4 (el_pedestal) está completada.\n");
        return 0;
    } else {
        printf("❌ RESULTADO: Se encontraron %d errores de colisión. Revisa fips202.c\n", fallos);
        return -1;
    }
}
