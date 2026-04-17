#include "poly.h"

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

