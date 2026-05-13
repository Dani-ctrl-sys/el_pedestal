/**
 * @file packing.c
 * @brief Implementación de la serialización y bit-packing para ML-DSA.
 */

#include "packing.h"

/**
 * @brief Empaqueta un polinomio t1 (10 bits por coeficiente).
 * 
 * Cada 4 coeficientes de 10 bits se serializan en 5 bytes.
 */

void polyt1_pack(uint8_t *r, const poly *a) {
    for(int i = 0; i < N/4; i++) {
        // Byte 0: Los 8 bits más bajos de a0
        // Byte 0: bits[0-7] de coeffs[0]
        r[5*i+0] = (a->coeffs[4*i+0] >> 0) & 0xFF;
        // Byte 1: bits[8-9] de coeffs[0] y bits[0-5] de coeffs[1]
        r[5*i+1] = ((a->coeffs[4*i+0] >> 8) | (a->coeffs[4*i+1] << 2)) & 0xFF;
        // Byte 2: bits[6-9] de coeffs[1] y bits[0-3] de coeffs[2]
        r[5*i+2] = ((a->coeffs[4*i+1] >> 6) | (a->coeffs[4*i+2] << 4)) & 0xFF;
        // Byte 3: bits[4-9] de coeffs[2] y bits[0-1] de coeffs[3]
        r[5*i+3] = ((a->coeffs[4*i+2] >> 4) | (a->coeffs[4*i+3] << 6)) & 0xFF;
        // Byte 4: bits[2-9] de coeffs[3]
        r[5*i+4] = (a->coeffs[4*i+3] >> 2) & 0xFF;
    }
}

/**
 * @brief Desempaqueta un polinomio t1 (10 bits por coeficiente).
 */
void polyt1_unpack(poly *r, const uint8_t *a) {
    for(int i = 0; i < N/4; i++) {
        // coeffs[0]: Byte 0 y bits[0-1] de Byte 1
        r->coeffs[4*i+0] = ((a[5*i+0] >> 0) | ((uint32_t)a[5*i+1] << 8)) & 0x3FF;
        // coeffs[1]: bits[2-7] de Byte 1 y bits[0-3] de Byte 2
        r->coeffs[4*i+1] = ((a[5*i+1] >> 2) | ((uint32_t)a[5*i+2] << 6)) & 0x3FF;
        // coeffs[2]: bits[4-7] de Byte 2 y bits[0-5] de Byte 3
        r->coeffs[4*i+2] = ((a[5*i+2] >> 4) | ((uint32_t)a[5*i+3] << 4)) & 0x3FF;
        // coeffs[3]: bits[6-7] de Byte 3 y Byte 4
        r->coeffs[4*i+3] = ((a[5*i+3] >> 6) | ((uint32_t)a[5*i+4] << 2)) & 0x3FF;
    }
}

/**
 * @brief Serializa la clave pública pk = (rho || t1).
 */
void pack_pk(uint8_t pk[CRYPTO_PUBLICKEYBYTES], const uint8_t rho[32], const polyveck *t1) {
    // Concatenación de la semilla rho
    for(int i=0; i<32; i++){
        pk[i] = rho[i];
    }
    
    // Serialización del vector t1
    for(int i=0; i<K; i++){
        polyt1_pack(&pk[32+i*POLYT1_PACKEDBYTES], &t1->vec[i]);
    }
}

/* 
 * Desempaqueta un array de Llave Pública para restaurar `rho` y `t1`. 
 */
void unpack_pk(uint8_t rho[32], polyveck *t1, const uint8_t pk[CRYPTO_PUBLICKEYBYTES]) {
    // TODO: 1. Extraer los primeros 32 bytes de pk hacia rho
    for(int i=0; i<32; i++){
        rho[i] = pk[i];
    }

    // TODO: 2. Bucle K para extraer los K polinomios usando polyt1_unpack
    // Pista para el offset de lectura: &pk[32 + i * POLYT1_PACKEDBYTES]
    for(int i=0; i<K; i++){
        polyt1_unpack(&t1->vec[i], &pk[32+i*POLYT1_PACKEDBYTES]);
    }
}

/**
 * @brief Serializa un polinomio eta (ETA=2, 3 bits por coeficiente).
 * 
 * Mapea el rango [-2, 2] a [0, 4] y empaqueta 8 coeficientes en 3 bytes.
 */
void polyeta_pack(uint8_t *r, const poly *a) {
    uint8_t t[8];
    
    for(int i = 0; i < N/8; i++) {
        // Normalización al rango positivo [0, 4]
        t[0] = ETA - a->coeffs[8*i+0];
        t[1] = ETA - a->coeffs[8*i+1];
        t[2] = ETA - a->coeffs[8*i+2];
        t[3] = ETA - a->coeffs[8*i+3];
        t[4] = ETA - a->coeffs[8*i+4];
        t[5] = ETA - a->coeffs[8*i+5];
        t[6] = ETA - a->coeffs[8*i+6];
        t[7] = ETA - a->coeffs[8*i+7];

        r[3*i+0] = ((t[0] >> 0) | (t[1] << 3) | (t[2] << 6)) & 0xFF;
        r[3*i+1] = ((t[2] >> 2) | (t[3] << 1) | (t[4] << 4) | (t[5] << 7)) & 0xFF;
        r[3*i+2] = ((t[5] >> 1) | (t[6] << 2) | (t[7] << 5)) & 0xFF;
    }
}

void polyeta_unpack(poly *r, const uint8_t *a) {
    for(int i = 0; i < N/8; i++) {
        // TODO: Proceso inverso. Desempaqueta 3 bytes en 8 variables de 3 bits.
        // Al final, ¡acuérdate de restarlo a ETA para volver a los números negativos originales!
        // (La máscara para asegurar que coges solo 3 bits es: & 0x07)
        
        // Ejemplo para el primero (t0 estaba entero al principio del byte 0):
        // Coeficiente 0 y 1 están perfectos
        r->coeffs[8*i+0] = ETA - ((a[3*i+0] >> 0) & 0x07);
        r->coeffs[8*i+1] = ETA - ((a[3*i+0] >> 3) & 0x07);

        // Coeficiente 2: Fíjate en los paréntesis añadidos en la unión
        r->coeffs[8*i+2] = ETA - (((a[3*i+0] >> 6) | (a[3*i+1] << 2)) & 0x07);

        // Coeficiente 3 y 4 están perfectos
        r->coeffs[8*i+3] = ETA - ((a[3*i+1] >> 1) & 0x07);
        r->coeffs[8*i+4] = ETA - ((a[3*i+1] >> 4) & 0x07);

        // Coeficiente 5: Paréntesis añadidos en la unión
        r->coeffs[8*i+5] = ETA - (((a[3*i+1] >> 7) | (a[3*i+2] << 1)) & 0x07);

        // Coeficiente 6 y 7 están perfectos
        r->coeffs[8*i+6] = ETA - ((a[3*i+2] >> 2) & 0x07);
        r->coeffs[8*i+7] = ETA - ((a[3*i+2] >> 5) & 0x07);
    }
}

/**
 * @brief Serializa un polinomio t0 (13 bits por coeficiente).
 * 
 * Cada 8 coeficientes de 13 bits se serializan en 13 bytes.
 */

void polyt0_pack(uint8_t *r, const poly *a) {
    uint32_t t[8];
    for(int i = 0; i < N/8; i++) {
        // Mapeamos los coeficientes sumando 2^12 (4096) para que sean positivos [0, 8191]
        t[0] = (1 << 12) + a->coeffs[8*i+0];
        t[1] = (1 << 12) + a->coeffs[8*i+1];
        t[2] = (1 << 12) + a->coeffs[8*i+2];
        t[3] = (1 << 12) + a->coeffs[8*i+3];
        t[4] = (1 << 12) + a->coeffs[8*i+4];
        t[5] = (1 << 12) + a->coeffs[8*i+5];
        t[6] = (1 << 12) + a->coeffs[8*i+6];
        t[7] = (1 << 12) + a->coeffs[8*i+7];

        // Serialización bit-pack de 13 bits en 13 bytes
        r[13*i+ 0] =  t[0];
        r[13*i+ 1] = (t[0] >>  8) | (t[1] << 5);
        r[13*i+ 2] = (t[1] >>  3);
        r[13*i+ 3] = (t[1] >> 11) | (t[2] << 2);
        r[13*i+ 4] = (t[2] >>  6) | (t[3] << 7);
        r[13*i+ 5] = (t[3] >>  1);
        r[13*i+ 6] = (t[3] >>  9) | (t[4] << 4);
        r[13*i+ 7] = (t[4] >>  4);
        r[13*i+ 8] = (t[4] >> 12) | (t[5] << 1);
        r[13*i+ 9] = (t[5] >>  7) | (t[6] << 6);
        r[13*i+10] = (t[6] >>  2);
        r[13*i+11] = (t[6] >> 10) | (t[7] << 3);
        r[13*i+12] = (t[7] >>  5);
    }
}

void polyt0_unpack(poly *r, const uint8_t *a) {
    for(int i = 0; i < N/8; i++) {
        // Extraemos bloques de 13 bytes para sacar 8 coeficientes de 13 bits
        // Extraemos los bits con la máscara 0x1FFF y finalmente restamos 2^12 (4096)
        r->coeffs[8*i+0] = (int32_t)(((a[13*i+0] >> 0) | (a[13*i+1] <<  8)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+1] = (int32_t)(((a[13*i+1] >> 5) | (a[13*i+2] <<  3) | (a[13*i+3] << 11)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+2] = (int32_t)(((a[13*i+3] >> 2) | (a[13*i+4] <<  6)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+3] = (int32_t)(((a[13*i+4] >> 7) | (a[13*i+5] <<  1) | (a[13*i+6] <<  9)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+4] = (int32_t)(((a[13*i+6] >> 4) | (a[13*i+7] <<  4) | (a[13*i+8] << 12)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+5] = (int32_t)(((a[13*i+8] >> 1) | (a[13*i+9] <<  7)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+6] = (int32_t)(((a[13*i+9] >> 6) | (a[13*i+10] << 2) | (a[13*i+11] << 10)) & 0x1FFF) - (1 << 12);
        r->coeffs[8*i+7] = (int32_t)(((a[13*i+11] >> 3) | (a[13*i+12] << 5)) & 0x1FFF) - (1 << 12);
    }
}

void pack_sk(uint8_t sk[CRYPTO_SECRETKEYBYTES], 
             const uint8_t rho[32], const uint8_t K_key[32], const uint8_t tr[64],
             const polyvecl *s1, const polyveck *s2, const polyveck *t0) {
    
    unsigned int i;

    for(i=0; i<32; i++) sk[i] = rho[i];
    sk += 32;
    for(i=0; i<32; i++) sk[i] = K_key[i];
    sk += 32;
    for(i=0; i<64; i++) sk[i] = tr[i];
    sk += 64;

    for(i=0; i<L; i++) {
        polyeta_pack(sk + i * POLYETA_PACKEDBYTES, &s1->vec[i]);
    }
    sk += L * POLYETA_PACKEDBYTES;

    for(i=0; i<K; i++) {
        polyeta_pack(sk + i * POLYETA_PACKEDBYTES, &s2->vec[i]);
    }
    sk += K * POLYETA_PACKEDBYTES;

    for(i=0; i<K; i++) {
        polyt0_pack(sk + i * POLYT0_PACKEDBYTES, &t0->vec[i]);
    }
}

/**
 * @brief Deserializa el vector s1, s2 y t0 a partir de la clave secreta sk.
 */
void unpack_sk(uint8_t rho[32], uint8_t K_key[32], uint8_t tr[64], 
               polyvecl *s1, polyveck *s2, polyveck *t0, 
               const uint8_t *sk) {
    
    for(int i=0; i<32; i++) rho[i]   = sk[i];
    for(int i=0; i<32; i++) K_key[i] = sk[32 + i];
    for(int i=0; i<64; i++) tr[i]    = sk[64 + i];
    
    sk += 128;
    
    for(int i=0; i<L; i++) {
        polyeta_unpack(&s1->vec[i], sk + i * POLYETA_PACKEDBYTES);
    }
    sk += L * POLYETA_PACKEDBYTES;
    
    for(int i=0; i<K; i++) {
        polyeta_unpack(&s2->vec[i], sk + i * POLYETA_PACKEDBYTES);
    }
    sk += K * POLYETA_PACKEDBYTES;
    
    for(int i=0; i<K; i++) {
        polyt0_unpack(&t0->vec[i], sk + i * POLYT0_PACKEDBYTES);
    }
}

/* Empaqueta un polinomio z (rango 18 bits para GAMMA1=2^17) */
void polyz_pack(uint8_t *r, const poly *a) {
    uint32_t t[4];
    for(int i = 0; i < N/4; i++) {
        // Mapeamos de [-GAMMA1, GAMMA1] a [0, 2*GAMMA1]
        t[0] = GAMMA1 - a->coeffs[4*i+0];
        t[1] = GAMMA1 - a->coeffs[4*i+1];
        t[2] = GAMMA1 - a->coeffs[4*i+2];
        t[3] = GAMMA1 - a->coeffs[4*i+3];

        // 72 bits = 9 bytes
        r[9*i+0] = (t[0] >> 0)  & 0xFF;
        r[9*i+1] = (t[0] >> 8)  & 0xFF;
        r[9*i+2] = ((t[0] >> 16) | (t[1] << 2)) & 0xFF;
        r[9*i+3] = (t[1] >> 6)  & 0xFF;
        r[9*i+4] = (t[1] >> 14) & 0xFF;
        r[9*i+5] = (t[2] >> 0)  & 0xFF;
        r[9*i+6] = (t[2] >> 8)  & 0xFF;
        r[9*i+7] = ((t[2] >> 16) | (t[3] << 2)) & 0xFF;
        r[9*i+8] = (t[3] >> 6)  & 0xFF;
    }
}

/* Ensambla la firma completa: c (32) || z (L*576) || h (84) */
void pack_sig(uint8_t *sig, const uint8_t c[32], const polyvecl *z, const polyveck *h) {
    unsigned int i, j, k;

    // 1. Copiar c
    for(i=0; i<32; i++) sig[i] = c[i];
    sig += 32;

    // 2. Empaquetar z
    for(i=0; i<L; i++) {
        polyz_pack(sig + i * POLYZ_PACKEDBYTES, &z->vec[i]);
    }
    sig += L * POLYZ_PACKEDBYTES;

    // 3. Empaquetar hints h (Formato FIPS 204: indices + fin de polinomios)
    // Inicializamos a cero los 84 bytes de h (OMEGA + K)
    for(i=0; i < OMEGA + K; i++) sig[i] = 0;

    k = 0;
    for(i=0; i < K; i++) {
        for(j=0; j < N; j++) {
            if(h->vec[i].coeffs[j] != 0) {
                sig[k++] = j; // Almacenamiento del índice del coeficiente no nulo
            }
        }
    sig[OMEGA + i] = k; // Límite del polinomio i en el vector h
    }
}

/* Empaqueta la parte alta w1 (6 bits por coeficiente para Modo 2) */
void polyw1_pack(uint8_t *r, const poly *a) {
    for(int i = 0; i < N/4; i++) {
        r[3*i+0] = (a->coeffs[4*i+0] >> 0) | (a->coeffs[4*i+1] << 6);
        r[3*i+1] = (a->coeffs[4*i+1] >> 2) | (a->coeffs[4*i+2] << 4);
        r[3*i+2] = (a->coeffs[4*i+2] >> 4) | (a->coeffs[4*i+3] << 2);
    }
}

void polyveck_pack_w1(uint8_t *r, const polyveck *v) {
    for(int i = 0; i < K; i++) {
        polyw1_pack(r + i * 192, &v->vec[i]);
    }
}
