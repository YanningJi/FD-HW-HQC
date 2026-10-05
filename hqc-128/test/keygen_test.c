#include <stdio.h>
#include "test_utils.h"
#include "hqc.h"
#include "parameters.h"
#include "parsing.h"
#include "shake_prng.h"
#include "gf2x.h"
#include "code.h"
#include "vector.h"
#include <stdint.h>
#include <string.h>



int main() {
    seedexpander_state sk_seedexpander;
    seedexpander_state pk_seedexpander;
    uint8_t sk_seed[SEED_BYTES] = {0};
    uint8_t pk_seed[SEED_BYTES] = {0};
    static __m256i h_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i y_256[VEC_N_256_SIZE_64 >> 2];   
    static __m256i x_256[VEC_N_256_SIZE_64 >> 2];
    static uint64_t s[VEC_N_256_SIZE_64];
    static __m256i tmp_256[VEC_N_256_SIZE_64 >> 2];

    #ifdef __STDC_LIB_EXT1__
        memset_s(x_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset_s(y_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset_s(h_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
    #else
        memset(x_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset(y_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset(h_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
    #endif


    char str[SEED_BYTES * 2];
    
    printf("Please input the public seed in hex:\n");
    if(scanf("%s", str)){};
    hex_str_to_bytes(pk_seed, str, SEED_BYTES);

    printf("Please input the secret seed in hex:\n");
    if(scanf("%s", str)){};
    hex_str_to_bytes(sk_seed, str, SEED_BYTES);


    // Create seed_expanders for public key and secret key
    // shake_prng(sk_seed, SEED_BYTES);
    seedexpander_init(&sk_seedexpander, sk_seed, SEED_BYTES);

    // shake_prng(pk_seed, SEED_BYTES);
    seedexpander_init(&pk_seedexpander, pk_seed, SEED_BYTES);

    // Compute secret key
    vect_set_random_fixed_weight(&sk_seedexpander, x_256, PARAM_OMEGA);

    printf("Polynomial x:\n");
    vect_print((uint64_t *) x_256, VEC_N_SIZE_BYTES);

    vect_set_random_fixed_weight(&sk_seedexpander, y_256, PARAM_OMEGA);

    printf("\n\nPolynomial y:\n");
    vect_print((uint64_t *) y_256, VEC_N_SIZE_BYTES);

    // Compute public key
    vect_set_random(&pk_seedexpander, (uint64_t *) h_256);

    printf("\n\nPolynomial h:\n");
    vect_print((uint64_t *) h_256, VEC_N_SIZE_BYTES);

    vect_mul(tmp_256, y_256, h_256);

    printf("\n\nMultiplication result:\n");
    vect_print((uint64_t *) tmp_256, VEC_N_SIZE_BYTES);


    vect_add(s, (uint64_t *) x_256, (uint64_t *) tmp_256, VEC_N_256_SIZE_64);

    printf("\n\nAddition result:\n");
    vect_print((uint64_t *) s, VEC_N_SIZE_BYTES);

    printf("\n");
}