#include "poly.h"
#include "fips202.h"

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
