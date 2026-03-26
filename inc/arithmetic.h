#ifndef ARITHMETIC_H
#define ARITHMETIC_H

#include <stdint.h>

#define Q 8380417
#define QINV 58728449

int32_t conditional_subq(int32_t a);
int32_t caddq(int32_t a);
int32_t montgomery_reduce(int64_t a);

#endif