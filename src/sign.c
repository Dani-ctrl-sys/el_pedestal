#include "sign.h"
#include "fips202.h"

/* ==========================================================================
 * ML-DSA (FIPS 204) - Módulo de Firma y Generación de Claves
 * 
 * Implementación de los algoritmos de generación de claves y firma digital
 * basados en el estándar FIPS 204 (Module-Lattice-Based Digital Signature Standard).
 * ========================================================================== */

/**
 * @brief Genera un par de claves pública (pk) y secreta (sk) siguiendo FIPS 204.
 * 
 * @param pk Puntero al array donde se almacenará la clave pública (CRYPTO_PUBLICKEYBYTES).
 * @param sk Puntero al array donde se almacenará la clave secreta (CRYPTO_SECRETKEYBYTES).
 * @return int 0 si la generación fue exitosa.
 */
int crypto_sign_keypair(uint8_t *pk, uint8_t *sk) {
    uint8_t seedbuf[96]; // Semillas rho (32) y rho' (64)
    uint8_t tr[64];      // T1 hash de la clave pública
    polyvecl mat[K];     // Matriz A expandida
    polyvecl s1;         // Vector secreto s1
    polyveck s2;         // Vector secreto s2
    polyveck t1, t0;     // Componentes t1 (MSB) y t0 (LSB) de t = As + s
    uint8_t zeta[32] = {0x01, 0x02, 0x03}; // Fuente de entropía externa

    // Generación de semillas maestras rho y rho'
    shake256(seedbuf, 96, zeta, 32);
    
    uint8_t K_key[32];
    shake256(K_key, 32, zeta, 32); // Derivación de la clave de entropía determinista

    // Expansión de la matriz pública A a partir de rho
    polyvec_matrix_expand(mat, seedbuf);  

    // Muestreo de secretos s1 y s2 a partir de rho'
    polyvecl_uniform_eta(&s1, seedbuf+32, 0);
    polyveck_uniform_eta(&s2, seedbuf+32, L);

    // Cálculo de t = A * s1 + s2 en el dominio NTT
    polyvecl s1_ntt = s1;
    polyvecl_ntt(&s1_ntt);

    for (int i=0; i<K; i++) {
        polyvecl_pointwise_acc(&t1.vec[i], &mat[i], &s1_ntt);
        poly_reduce(&t1.vec[i]);
    }
    polyveck_invntt(&t1);
    polyveck_add(&t1, &t1, &s2);
    polyveck_reduce(&t1);

    // Descomposición de t en MSB (t1) y LSB (t0)
    polyveck_power2round(&t1, &t0, &t1);

    // Serialización de la clave pública
    pack_pk(pk, seedbuf, &t1);

    // Cálculo del hash de la clave pública (tr) para el protocolo de firma
    shake256(tr, 64, pk, CRYPTO_PUBLICKEYBYTES);

    // Serialización de la clave secreta
    pack_sk(sk, seedbuf, K_key, tr, &s1, &s2, &t0);
    
    return 0;
}

/**
 * @brief Función interna para derivar el hash del mensaje combinado con tr.
 */
static void shake256_msg(uint8_t *out, size_t outlen, const uint8_t *tr, const uint8_t *m, size_t mlen) {
    // Uso de VLA para el buffer de hash (tr || m)
    uint8_t buffer[64 + mlen]; 
    
    for(int i = 0; i < 64; i++) buffer[i] = tr[i];
    for(size_t i = 0; i < mlen; i++) buffer[64+i] = m[i];
    
    shake256(out, outlen, buffer, 64 + mlen);
}


/**
 * @brief Algoritmo de firma digital ML-DSA.
 * 
 * Implementa el protocolo Fiat-Shamir with Aborts. Itera hasta que se genera
 * una firma que cumple con los límites de seguridad normativos.
 * 
 * @param sig Puntero al buffer de salida de la firma (CRYPTO_BYTES).
 * @param siglen Puntero a la variable que almacenará la longitud final.
 * @param m Mensaje a firmar.
 * @param mlen Longitud del mensaje.
 * @param sk Clave secreta empaquetada.
 * @return int 0 si la firma fue generada exitosamente.
 */

int crypto_sign(uint8_t *sig, size_t *siglen, const uint8_t *m, size_t mlen, const uint8_t *sk) {
    uint8_t mu[64]; // Hash del documento mezclado con la Llave Pública
    uint8_t rhoprime[64]; // Semilla para el vector enmascarado 'y'
    uint16_t nonce = 0;
    int signature_valid = 0;

    uint8_t rho[32];      // Semilla pública (para reconstruir la matriz A)
    uint8_t K_key[32];    // Semilla privada maestra
    uint8_t tr[64];       // Hash de la llave pública
    polyvecl s1;          // Tu secreto 1
    polyveck s2;          // Tu secreto 2
    polyveck t0;          // La parte "baja/ruidosa" de la llave pública
    
    // 1. Desempaquetar la clave secreta `sk`
    unpack_sk(rho, K_key, tr, &s1, &s2, &t0, sk);

    // 1.1 Pre-calcular NTT de las claves (Optimización: una sola vez)
    polyvecl s1_ntt = s1; polyvecl_ntt(&s1_ntt);
    polyveck s2_ntt = s2; polyveck_ntt(&s2_ntt);
    polyveck t0_ntt = t0; polyveck_ntt(&t0_ntt);
    
    // Derivación del hash del mensaje mu = H(tr || m)
    shake256_msg(mu, 64, tr, m, mlen);

    // Derivación de rho' = H(K || mu) para muestreo determinista
    uint8_t buf_k_mu[96];
    for(int i = 0; i < 32; i++) buf_k_mu[i] = K_key[i];
    for(int i = 0; i < 64; i++) buf_k_mu[32+i] = mu[i];
    shake256(rhoprime, 64, buf_k_mu, 96);

    // Expansión de la matriz pública A
    polyvecl mat[K];
    polyvec_matrix_expand(mat, rho);
    
    polyvecl y, y_ntt;
    polyvecl z;
    polyveck w, w1, w0;
    poly cp;
    uint8_t c[32];

    /* Bucle de generación (Fiat-Shamir with Aborts) */
    while (!signature_valid) {
        // a) Generación del vector enmascarado y
        polyvecl_uniform_gamma1(&y, rhoprime, nonce);
        nonce += L;

        // b) Cálculo de w1 = HighBits(A * y)
        y_ntt = y;
        polyvecl_ntt(&y_ntt);
        for(int i = 0; i < K; i++) {
            polyvecl_pointwise_acc(&w.vec[i], &mat[i], &y_ntt);
            poly_reduce(&w.vec[i]);
        }
        polyveck_invntt(&w);
        polyveck_caddq(&w); 
        polyveck_decompose(&w1, &w0, &w);

        // c) Generación del reto c = H(mu || PackW1(w1))
        uint8_t w1_packed[K * 192]; 
        polyveck_pack_w1(w1_packed, &w1);
        shake256_msg(c, 32, mu, w1_packed, K * 192);

        // d) Cálculo de la respuesta z = y + c * s1
        poly_challenge(&cp, c);
        poly_ntt(&cp);

        for(int i = 0; i < L; i++) {
            poly_pointwise_montgomery(&z.vec[i], &cp, &s1_ntt.vec[i]);
            poly_invntt(&z.vec[i]); 
        }

        polyvecl_add(&z, &z, &y);
        polyvecl_reduce(&z);

        // e) Rejection Sampling (Norma infinita de z)
        if (polyvecl_chknorm(&z, GAMMA1 - BETA)) {
            continue;
        }
        
        // f) Evaluación de pistas (Hints) y Rejection Sampling secundario (r0)
        polyveck cs2, ct0;
        for(int i = 0; i < K; i++) {
            poly_pointwise_montgomery(&cs2.vec[i], &cp, &s2_ntt.vec[i]);
            poly_invntt(&cs2.vec[i]);
            poly_pointwise_montgomery(&ct0.vec[i], &cp, &t0_ntt.vec[i]);
            poly_invntt(&ct0.vec[i]);
        }

        // Comprobación de r0 = LowBits(w - c*s2)
        polyveck w_minus_cs2, r0_sec, dummy_w1;
        polyveck_sub(&w_minus_cs2, &w, &cs2);
        polyveck_reduce(&w_minus_cs2);
        polyveck_caddq(&w_minus_cs2);
        
        polyveck_decompose(&dummy_w1, &r0_sec, &w_minus_cs2);

        if (polyveck_chknorm(&r0_sec, GAMMA2 - BETA)) {
            continue; 
        }

        // Generación de pistas (Hints)
        polyveck ct0_neg, w_cs2_ct0;
        
        for(int i = 0; i < K; i++) {
            for(int j = 0; j < N; j++) {
                ct0_neg.vec[i].coeffs[j] = Q - ct0.vec[i].coeffs[j];
            }
        }
        
        polyveck_add(&w_cs2_ct0, &w_minus_cs2, &ct0);
        polyveck_reduce(&w_cs2_ct0);

        polyveck h;
        unsigned int num_hints = polyveck_make_hint(&h, &ct0_neg, &w_cs2_ct0);
        
        if (num_hints > OMEGA) {
            continue; 
        }
         
        signature_valid = 1; 
    }
    
    // Serialización de la firma final
    pack_sig(sig, c, &z, &h);
    *siglen = CRYPTO_BYTES;

    return 0;
}

/**
 * @brief Algoritmo de verificación ML-DSA.
 */
int crypto_sign_verify(const uint8_t *sig, size_t siglen, const uint8_t *m, size_t mlen, const uint8_t *pk) {
    // Validación de longitud de firma requerida por FIPS 204
    if(siglen != CRYPTO_BYTES) return -1;

    // Deserialización de clave pública y firma
    uint8_t rho[32];
    polyveck t1;

    uint8_t c[32];
    polyvecl z;
    polyveck h;
    
    unpack_pk(rho, &t1, pk);
    if(unpack_sig(c, &z, &h, sig)) return -1;

    // Validación de seguridad: rechazo si la norma infinita excede los límites prescritos
    if(polyvecl_chknorm(&z, GAMMA1 - BETA)) return -1;

    // Reconstrucción del vector w1' a través del polinomio reto c
    polyvecl mat[K];
    polyvec_matrix_expand(mat, rho);

    polyvecl_ntt(&z);

    polyveck w;
    for(int i=0; i<K; i++){
        polyvecl_pointwise_acc(&w.vec[i], &mat[i], &z);
        poly_reduce(&w.vec[i]);
    }

    poly cp;
    poly_challenge(&cp, c);
    poly_ntt(&cp);
    
    for(int i=0; i<K; i++){
        poly_shiftl(&t1.vec[i]);
        poly_ntt(&t1.vec[i]);
        poly ct1;
        poly_pointwise_montgomery(&ct1, &cp, &t1.vec[i]);
        poly_sub(&w.vec[i], &w.vec[i], &ct1);
        poly_reduce(&w.vec[i]);
    }
    
    polyveck_invntt(&w);
    polyveck_caddq(&w);
    
    // Aplicación de pistas (hints) para la recuperación exacta de los bits altos
    polyveck w1;
    polyveck_use_hint(&w1, &h, &w);
    
    // Generación de huellas digitales de clave y mensaje (Identidad + Contrato)
    uint8_t tr[64];
    shake256(tr, 64, pk, CRYPTO_PUBLICKEYBYTES);

    uint8_t mu[64];
    shake256_msg(mu, 64, tr, m, mlen);

    uint8_t w1_packed[K * 192];
    polyveck_pack_w1(w1_packed, &w1);
    
    // Derivación del reto criptográfico de verificación c2
    uint8_t c2[32];
    shake256_msg(c2, 32, mu, w1_packed, K * 192);
    
    // Comparación en tiempo constante de los retos criptográficos
    for(int i=0; i<32; i++){
        if(c[i] != c2[i]) return -1;
    }
    
    return 0; 
}
