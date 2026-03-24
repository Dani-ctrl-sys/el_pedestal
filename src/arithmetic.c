#include "arithmetic.h"

int32_t conditional_subq(int32_t a) {
    int32_t res = a - Q;
    int32_t mask = res >> 31;
    
    // Si res < 0 (máscara de unos), sumamos Q de vuelta.
    // Si res >= 0 (máscara de ceros), sumamos 0.
    return res + (Q & mask);
}