#ifndef HYLO_MATH_H
#define HYLO_MATH_H

#include <stdint.h>

int64_t hy_abs_i(int64_t n);
double hy_abs_f(double n);
int64_t hy_min_i(int64_t a, int64_t b);
int64_t hy_max_i(int64_t a, int64_t b);
int64_t hy_clamp_i(int64_t v, int64_t lo, int64_t hi);
int64_t hy_pow_i(int64_t base, int64_t exp);

#endif
