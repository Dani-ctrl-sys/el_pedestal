#include "poly.h"
#include "fips202.h"
#include "ntt.h"

void poly_add(poly *r, const poly *a, const poly *b) {
    for(int i=0; i<N; i++){
        r->coeffs[i] = a->coeffs[i] + b->coeffs[i];
    }
}

void poly_sub(poly *r, const poly *a, const poly *b) {
    for(int i=0; i<N; i++){
        r->coeffs[i] = a->coeffs[i] - b->coeffs[i];
    }
}

void poly_reduce(poly *a){
    for(int i=0; i<N; i++){
        a->coeffs[i] = barrett_reduce(a->coeffs[i]);
    }
}

void poly_caddq(poly *a){
    for(int i=0; i<N; i++){
        a->coeffs[i] = caddq(a->coeffs[i]);
    }
}

void power2round(int32_t *r1, int32_t *r0, int32_t a) {
    int32_t a_pos = caddq(a);

    *r0 = a_pos - ((a_pos + (1 << (D - 1))) >> D) * (1 << D);
    *r1 = (a_pos - *r0) >> D;
}

void poly_power2round(poly *r1, poly *r0, const poly *a){
    for(int i=0; i<N; i++){
        power2round(&(r1->coeffs[i]), &(r0->coeffs[i]), a->coeffs[i]);
    }
}

void decompose(int32_t *r1, int32_t *r0, int32_t a) {
    int32_t a_pos = caddq(a);
    
    *r0 = a_pos % (2 * GAMMA2);
    if (*r0 > GAMMA2)
        *r0 -= (2 * GAMMA2);
    
    if (a_pos - *r0 == Q - 1) {
        *r1 = 0;
        *r0 = *r0 - 1;
    } else {
        *r1 = (a_pos - *r0) / (2 * GAMMA2);
    }
}

int32_t  highbits(int32_t a){
    int32_t r0, r1;
    decompose(&r1, &r0, a);
    return r1;
}

int32_t lowbits(int32_t a){
    int32_t r0, r1;
    decompose(&r1, &r0, a);
    return r0;
}

void poly_decompose(poly *r1, poly *r0, const poly *a){
    for(int i=0; i<N; i++){
        decompose(&(r1->coeffs[i]), &(r0->coeffs[i]), a->coeffs[i]);
    }
}

void poly_highbits(poly *r, const poly *a){
    for(int i=0; i<N; i++){
        r->coeffs[i] = highbits(a->coeffs[i]);
    }
}

void poly_lowbits(poly *r, const poly *a){
    for(int i=0; i<N; i++){
        r->coeffs[i] = lowbits(a->coeffs[i]);
    }
}

int32_t make_hint(int32_t z, int32_t r){
    int32_t r1 = highbits(r);
    int32_t v1 = highbits(r + z);

    return (r1 != v1);
}

int32_t use_hint(int32_t h, int32_t r){
    int32_t a1, a0;
    decompose(&a1, &a0, r);

    if (h == 0) return a1;

    if (a0 > 0){
        return (a1 == M-1) ? 0 : a1 + 1;
    } else {
        return (a1 == 0) ? M-1 : a1 - 1;
    }
}

unsigned int polyveck_make_hint(polyveck *h, const polyveck *z, const polyveck *r){
    unsigned int i, j, s = 0;
    for(i=0; i<K; i++){
        for(j=0; j<N; j++){
            h->vec[i].coeffs[j] = make_hint(z->vec[i].coeffs[j], r->vec[i].coeffs[j]);
            s += h->vec[i].coeffs[j];
        }
    }
    return s;
}

void polyveck_use_hint(polyveck *r1, const polyveck *h, const polyveck *r) {
    unsigned int i, j;
    for (i = 0; i < K; ++i) {
        for (j = 0; j < N; ++j) {
            r1->vec[i].coeffs[j] = use_hint(h->vec[i].coeffs[j], r->vec[i].coeffs[j]);
        }
    }
}

void polyveck_add(polyveck *r, const polyveck *a, const polyveck *b) {
    unsigned int i;
    for (i = 0; i < K; ++i) {
        poly_add(&r->vec[i], &a->vec[i], &b->vec[i]);
    }
}

void polyveck_sub(polyveck *r, const polyveck *a, const polyveck *b) {
    unsigned int i;
    for (i = 0; i < K; ++i) {
        poly_sub(&r->vec[i], &a->vec[i], &b->vec[i]);
    }
}

void polyveck_reduce(polyveck *v) {
    unsigned int i;
    for (i = 0; i < K; ++i) {
        poly_reduce(&v->vec[i]);
    }
}

void polyveck_caddq(polyveck *v) {
    unsigned int i;
    for (i = 0; i < K; ++i) {
        poly_caddq(&v->vec[i]);
    }
}

void polyveck_power2round(polyveck *r1, polyveck *r0, const polyveck *v) {
    unsigned int i;
    for (i = 0; i < K; ++i) {
        poly_power2round(&r1->vec[i], &r0->vec[i], &v->vec[i]);
    }
}

void polyveck_decompose(polyveck *r1, polyveck *r0, const polyveck *v) {
    unsigned int i;
    for (i = 0; i < K; ++i) {
        poly_decompose(&r1->vec[i], &r0->vec[i], &v->vec[i]);
    }
}

/*
 * load24_littleendian - Deserializa 3 bytes contiguos a un entero de 32 bits
 *                       en formato little-endian.
 *
 * Implementación byte a byte para garantizar portabilidad en arquitecturas
 * con restricciones de alineación de memoria (e.g. ARMv6-M, Cortex-M0).
 * Una lectura directa de uint32_t* en memoria no alineada provocaría un
 * HardFault en dichos procesadores.
 *
 * @param buf Puntero al primer byte del bloque de 3 bytes a deserializar.
 * @return    Entero de 24 bits significativos en los bits [23:0].
 */
static uint32_t load24_littleendian(const uint8_t *buf) {
    uint32_t r;
    
    r  = (uint32_t)buf[0];
    r |= (uint32_t)buf[1] << 8;
    r |= (uint32_t)buf[2] << 16;
    
    return r;
}

/*
 * poly_uniform - Genera un polinomio uniforme sobre Z_q mediante la
 *               expansión de una semilla con SHAKE-128 (ExpandA, FIPS 204 §4.2.2).
 *
 * Implementa el algoritmo ExpandA descrito en FIPS 204. A partir de una semilla
 * pública rho de 32 bytes y un nonce de 16 bits que identifica la posición (i,j)
 * en la matriz A, deriva de forma determinista un polinomio a ∈ R_q con
 * coeficientes distribuidos uniformemente en [0, q-1].
 *
 * El nonce se serializa en los bytes 32-33 del bloque de entrada a la esponja
 * en formato little-endian, garantizando que cada posición (i,j) de la matriz
 * produzca un polinomio estadísticamente independiente.
 *
 * El muestreo se realiza mediante Rejection Sampling sobre bloques de
 * SHAKE128_RATE (168) bytes: se extraen candidatos de 24 bits y se descartan
 * aquellos fuera del rango [0, q-1], asegurando una distribución exactamente
 * uniforme sin sesgo de reducción modular.
 *
 * @param a     Polinomio de salida.
 * @param seed  Semilla pública rho de 32 bytes.
 * @param nonce Identificador de posición (i << 8 | j) para la matriz A.
 */
void poly_uniform(poly *a, const uint8_t seed[32], uint16_t nonce) {
    /* Construcción del bloque de entrada: rho || nonce (34 bytes en total). */
    uint8_t in[34];
    for(int i = 0; i < 32; i++) {
        in[i] = seed[i];
    }
    in[32] = nonce & 0xFF;  /* Byte bajo del nonce. */
    in[33] = nonce >> 8;    /* Byte alto del nonce. */
    
    /* Inicialización y absorción en la esponja SHAKE-128. */
    keccak_state state;
    shake128_absorb(&state, in, 34);

    int ctr = 0;
    uint8_t buf[SHAKE128_RATE];
    
    /*
     * Bucle de exprimido: extrae bloques de SHAKE128_RATE bytes hasta completar
     * los N coeficientes válidos del polinomio.
     */
    while (ctr < N) {
        shake128_squeezeblocks(buf, 1, &state);
        
        int pos = 0;
        
        /*
         * Rejection Sampling: procesa el bloque actual en grupos de 3 bytes.
         * Se descarta cualquier candidato z >= q para garantizar uniformidad.
         * Los bytes residuales al final del bloque (< 3) se ignoran.
         */
        while (ctr < N && pos + 3 <= SHAKE128_RATE) {
            uint32_t z = load24_littleendian(&buf[pos]);
            
            if (z < Q) {
                a->coeffs[ctr] = z;
                ctr++;
            }
            
            pos += 3;
        }
    }
}

/*
 * poly_make_hint - Calcula el vector de hints para un polinomio.
 *
 * Para cada coeficiente i, determina si el hint es necesario comparando
 * HighBits(r[i]) con HighBits(r[i] + z[i]).
 *
 * Referencia escalar: make_hint(z, r)  →  línea 86 de este archivo.
 * Referencia vectorial: polyveck_make_hint  →  línea 106 de este archivo.
 *
 * @param h  Polinomio de salida: h[i] = 0 ó 1.
 * @param z  Polinomio de corrección (low-order part del error).
 * @param r  Polinomio base.
 * @return   Número total de hints activos (sum de h[i]).
 */
unsigned int poly_make_hint(poly *h, const poly *z, const poly *r) {
    int s = 0;
    for(int j=0; j<N; j++){
        h->coeffs[j] = make_hint(z->coeffs[j], r->coeffs[j]);
        s += h->coeffs[j];
    }
    return s;
}

/*
 * poly_use_hint - Corrige los HighBits de un polinomio usando los hints.
 *
 * Para cada coeficiente i, aplica UseHint(h[i], r[i]) para recuperar
 * el valor correcto de r1 sin necesidad de conocer z.
 *
 * Referencia escalar: use_hint(h, r)  →  línea 93 de este archivo.
 *
 * @param r1  Polinomio de salida: coeficientes corregidos (HighBits).
 * @param h   Polinomio de hints.
 * @param r   Polinomio original.
 */
void poly_use_hint(poly *r1, const poly *h, const poly *r) {
    for(int j=0; j<N; j++){
        r1->coeffs[j] = use_hint(h->coeffs[j], r->coeffs[j]);
    }
}

/*
 * polyvecl_ntt - Aplica la NTT in-place a todos los L polinomios del vector.
 *
 * Transforma cada polinomio de R_q al dominio NTT para permitir la
 * multiplicación punto a punto en O(N) en lugar de convolución en O(N²).
 * Prerequisito: los coeficientes de entrada deben estar en forma reducida.
 *
 * @param v Vector de L polinomios a transformar.
 */
void polyvecl_ntt(polyvecl *v) {
    unsigned int i;
    for(i=0; i<L; i++){
        poly_ntt(v->vec[i].coeffs);
    }
}

/*
 * polyvecl_invntt - Aplica la NTT inversa in-place a todos los L polinomios del vector.
 *
 * Transforma cada polinomio del dominio NTT de vuelta a R_q. Incluye el
 * factor de escala N^{-1} mod q absorbido en la tabla de zetas inversos.
 *
 * @param v Vector de L polinomios a transformar.
 */
void polyvecl_invntt(polyvecl *v) {
    unsigned int i;
    for(i=0; i<L; i++){
        poly_invntt(v->vec[i].coeffs);
    }
}

/*
 * polyveck_ntt - Aplica la NTT in-place a todos los K polinomios del vector.
 *
 * @param v Vector de K polinomios a transformar.
 */
void polyveck_ntt(polyveck *v) {
    unsigned int i;
    for(i=0; i<K; i++){
        poly_ntt(v->vec[i].coeffs);
    }
}

/*
 * polyveck_invntt - Aplica la NTT inversa in-place a todos los K polinomios del vector.
 *
 * @param v Vector de K polinomios a transformar.
 */
void polyveck_invntt(polyveck *v) {
    unsigned int i;
    for(i=0; i<K; i++){
        poly_invntt(v->vec[i].coeffs);
    }
}

/*
 * polyvecl_pointwise_acc - Producto punto acumulado en dominio NTT (FIPS 204 §4).
 *
 * Computa r = sum_{i=0}^{L-1} a[i] ⊙ b.vec[i] en dominio NTT, donde ⊙
 * denota la multiplicación coeficiente a coeficiente (poly_mul_pointwise).
 * Equivale a una fila del producto matricial A·s en la generación de claves.
 *
 * Precondición: a[0..L-1] y b->vec[0..L-1] deben estar en dominio NTT.
 * Postcondición: r contiene el producto punto en dominio NTT, sin reducir.
 *
 * @param r  Polinomio de salida: acumulador del producto punto.
 * @param a  Fila de la matriz A: array de L polinomios en dominio NTT.
 * @param b  Vector columna s: polyvecl de L polinomios en dominio NTT.
 */
void polyvecl_pointwise_acc(poly *r, const poly *a, const polyvecl *b) {
    poly tmp;
    poly_mul_pointwise(r->coeffs, a[0].coeffs, b->vec[0].coeffs);
    for(unsigned int i=1; i<L; i++){
        poly_mul_pointwise(tmp.coeffs, a[i].coeffs, b->vec[i].coeffs);
        poly_add(r, r, &tmp);
    }
}

/* ==========================================================================
 * MINITAREA 4.1 — Norma infinita (branch-free)
 *
 * Objetivo: determinar si un polinomio o vector supera el umbral B.
 *           Es el "semáforo" del bucle de abort en Sign.
 *
 * Matemática:
 *   ||a||∞ = max_i |a_i|
 *   Retorna 1 (fallo) si algún |a_i| >= B, 0 (ok) si todos < B.
 *
 * PISTA — Valor absoluto en constant-time (sin if/branch):
 *   int32_t t = x >> 31;       <- máscara: 0x00000000 si x>=0,
 *                                           0xFFFFFFFF si x<0
 *   t = x - (t & (2 * x));    <- si x<0: x - 2x = -x
 *                                  si x>=0: x - 0  = x
 *
 * PISTA — ¿Por qué branch-free?
 *   Los coeficientes de z = y + c·s1 son material secreto durante Sign.
 *   Un branch sobre ellos filtraría información por timing side-channel.
 *
 * Referencia FIPS 204: Algorithm 2 (Sign), paso de comprobación de ||z||∞.
 * ========================================================================== */

/*
 * poly_chknorm - Comprueba si la norma infinita de a supera el umbral B.
 *
 * @param a  Polinomio a auditar.
 * @param B  Umbral de norma (B < Q/2).
 * @return   1 si ||a||∞ >= B (fallo/abort), 0 si ||a||∞ < B (ok).
 */
int poly_chknorm(const poly *a, int32_t B) {
    int32_t t;
    int32_t abs_x;
    int32_t ret=0;

    for(int i=0; i<N; i++){
        int32_t x = a->coeffs[i];
        t = x>>31;
        abs_x = x - (t & (2*x));
        t = (B - 1 - abs_x) >> 31;
        ret |= t;
    }
    
    return ret & 1;
    
}

/*
 * polyvecl_chknorm - Versión vectorial: audita los L polinomios de v.
 *
 * @param v  Vector de L polinomios.
 * @param B  Umbral de norma.
 * @return   1 si algún ||v[i]||∞ >= B, 0 si todos están dentro.
 */
int polyvecl_chknorm(const polyvecl *v, int32_t B) {
    int ret = 0;
    for(int i=0; i<L; i++){
        ret |= poly_chknorm(&v->vec[i], B);
    }
    return ret;
}

/* ==========================================================================
 * Muestreo de la Distribución Binomial (ExpandS)
 *
 * Expande una semilla criptográfica de 64 bytes (rho') mediante SHAKE-256 
 * para muestrear un polinomio con coeficientes en la distribución centrada 
 * limitada por ETA.
 *
 * Implementa el Algoritmo 32 (ExpandS) de FIPS 204.
 * Para ETA=2, se extraen bloques de 4 bits, aplicando Rejection Sampling
 * para descartar valores >= 15 y asegurar una distribución estadísticamente
 * idéntica a la norma.
 * ========================================================================== */

/*
 * poly_uniform_eta - Genera un polinomio con coeficientes en [-ETA, ETA]
 *
 * @param a      Polinomio de salida
 * @param seed   Semilla secreta (rho') de 64 bytes
 * @param nonce  Identificador para el stream (normalmente se suma la posición en el vector)
 */
void poly_uniform_eta(poly *a, const uint8_t seed[64], uint16_t nonce) {
    uint8_t in[66]; // 64 bytes de semilla + 2 bytes de nonce
    int i;

    // 1. Preparar el buffer `in` con la semilla y el nonce (en little-endian).
    // TODO: Copia los 64 bytes de seed a in, y pon nonce en in[64] e in[65].
    for(i=0; i<64; i++){
        in[i] = seed[i];
    }

    in[64] = (uint8_t) nonce;
    in[65] = (uint8_t) (nonce >> 8);

    // 2. Inicializar la esponja SHAKE-256
    keccak_state state;
    shake256_absorb(&state, in, 66);

    // 3. Extraer bloques y aplicar rejection sampling
    uint8_t buf[SHAKE256_RATE];
    int ctr = 0; // Coeficientes válidos encontrados
    uint8_t t0, t1;

    while (ctr < N) {
        shake256_squeezeblocks(buf, 1, &state);

        for(i=0; i<SHAKE256_RATE && ctr<N; i++){
            t0 = buf[i] & 0x0F;

            if(t0<15){
                a->coeffs[ctr++] = 2 - (t0 % 5);
            }

            if(ctr < N){
                t1= buf[i] >> 4;
                if(t1<15){
                    a->coeffs[ctr++] = 2 - (t1 % 5);
                }
            }
        }
    }
}

/* ==========================================================================
 * Muestreo de Máscara Uniforme (ExpandMask)
 *
 * Expande una semilla secreta de 64 bytes (rho') y un nonce de 16 bits
 * empleando SHAKE-256 para generar el vector de máscara y.
 *
 * Implementa el Algoritmo 33 (ExpandMask) de FIPS 204.
 * El desempaquetado (bitunpack) extrae bits de forma contigua para preservar
 * el determinismo del flujo SHAKE. Para DILITHIUM_MODE=2 (GAMMA1 = 2^17), 
 * mapea bloques de 72 bits a 4 coeficientes de 18 bits, sin descartar
 * entropía residual.
 * ========================================================================== */

/*
 * poly_uniform_gamma1 - Genera un polinomio con coeficientes en [-GAMMA1, GAMMA1]
 *
 * @param a      Polinomio de salida
 * @param seed   Semilla secreta (rho') de 64 bytes
 * @param nonce  Identificador para el stream
 */
void poly_uniform_gamma1(poly *a, const uint8_t seed[64], uint16_t nonce) {
    uint8_t in[66];
    int i;
    
    // 1. Preparar buffer (igual que poly_uniform_eta)
    for(i=0; i<64; i++) {
        in[i] = seed[i];
    }
    in[64] = (uint8_t) nonce;
    in[65] = (uint8_t) (nonce >> 8);

    // 2. SHAKE-256 (ExpandMask usa SHAKE-256)
    keccak_state state;
    shake256_absorb(&state, in, 66);

    uint8_t buf[SHAKE256_RATE];
    int ctr = 0; // Coeficientes encontrados
    int pos = 0; // Posición dentro del bloque buf

    // Vamos a exprimir bloques hasta tener los N coeficientes
    while (ctr < N) {
        shake256_squeezeblocks(buf, 1, &state);
        pos = 0;
        uint32_t z0, z1, z2, z3;

        // Procesamos bloques de 9 bytes para sacar 4 coeficientes de 18 bits
        while (pos + 9 <= SHAKE256_RATE && ctr < N) {
            z0 = (uint32_t)buf[pos];
            z0 |= (uint32_t)buf[pos+1] << 8;
            z0 |= (uint32_t)(buf[pos+2] & 0x03) << 16;
            
            z1 = (uint32_t)(buf[pos+2] >> 2);
            z1 |= (uint32_t)buf[pos+3] << 6;
            z1 |= (uint32_t)(buf[pos+4] & 0x0F) << 14;
            
            z2 = (uint32_t)(buf[pos+4] >> 4);
            z2 |= (uint32_t)buf[pos+5] << 4;
            z2 |= (uint32_t)(buf[pos+6] & 0x3F) << 12;
            
            z3 = (uint32_t)(buf[pos+6] >> 6);
            z3 |= (uint32_t)buf[pos+7] << 2;
            z3 |= (uint32_t)buf[pos+8] << 10;

            a->coeffs[ctr++] = (int32_t)GAMMA1 - (int32_t)z0;
            if (ctr < N) a->coeffs[ctr++] = (int32_t)GAMMA1 - (int32_t)z1;
            if (ctr < N) a->coeffs[ctr++] = (int32_t)GAMMA1 - (int32_t)z2;
            if (ctr < N) a->coeffs[ctr++] = (int32_t)GAMMA1 - (int32_t)z3;
            
            pos += 9;
        }
    }
}

/* ==========================================================================
 * MINITAREA 4.2 — Norma Infinita Vectorial (Dimensión K)
 * ========================================================================== */
int polyveck_chknorm(const polyveck *v, int32_t B) {
    int ret = 0;
    for(int i = 0; i < K; i++){
        ret |= poly_chknorm(&v->vec[i], B);
    }
    return ret;
}

/* ==========================================================================
 * MINITAREA 4.3 — Desplazamiento Lógico (poly_shiftl)
 * ========================================================================== */
void poly_shiftl(poly *a) {
    for(int i = 0; i < N; i++){
        a->coeffs[i] <<= D;
    }
}

/* ==========================================================================
 * MINITAREA 4.4 — Envolturas de Expansión Vectorial
 * ========================================================================== */
void polyvecl_uniform_eta(polyvecl *v, const uint8_t seed[64], uint16_t nonce) {
    for(int i = 0; i < L; i++){
        poly_uniform_eta(&v->vec[i], seed, nonce + i);
    }
}

void polyveck_uniform_eta(polyveck *v, const uint8_t seed[64], uint16_t nonce) {
    for(int i = 0; i < K; i++){
        poly_uniform_eta(&v->vec[i], seed, nonce + i);
    }
}

/* ==========================================================================
 * MINITAREA 4.5 — Generador de la Matriz Pública A
 * ========================================================================== */
void polyvec_matrix_expand(polyvecl mat[K], const uint8_t rho[32]) {
    for(int i = 0; i < K; i++){
        for(int j = 0; j < L; j++){
            /* El nonce se forma con (fila << 8) + columna, según FIPS 204 */
            poly_uniform(&mat[i].vec[j], rho, (i << 8) + j);
        }
    }
}
