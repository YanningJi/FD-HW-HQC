#ifndef GF2X_H
#define GF2X_H

/**
 * @file gf2x.h
 * @brief Header file for gf2x.c
 */

#include <stdint.h>
#include <immintrin.h>

void reduce(__m256i *o, const __m256i *a);
void toom_3_mult(__m256i *C, const __m256i *A, const __m256i *B);
void vect_mul(__m256i *o, const __m256i *v1, const __m256i *v2);

#endif
