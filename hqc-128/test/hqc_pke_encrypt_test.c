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
    char str[(SEED_BYTES > PARAM_K ? SEED_BYTES : PARAM_K) * 2];
    uint8_t pk_seed[SEED_BYTES] = {0};
    uint8_t sk_seed[SEED_BYTES] = {0};
    uint8_t theta[SEED_BYTES];
    uint64_t m[VEC_K_SIZE_64];

    printf("Please input the public seed in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(pk_seed, str, SEED_BYTES);

    printf("Please input the secret seed in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(sk_seed, str, SEED_BYTES);

    printf("Please input theta in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(theta, str, SEED_BYTES);

    printf("Please input message in hex (%d bytes):\n", PARAM_K);
    if(scanf("%s", str)){};
    hex_str_to_bytes((uint8_t *) m, str, PARAM_K);


    seedexpander_state seedexpander;
    static __m256i h_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i s_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i r2_256[VEC_N_256_SIZE_64 >> 2];

    static __m256i r1_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i e_256[VEC_N_256_SIZE_64 >> 2];

    static __m256i tmp1_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i tmp2_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i tmp3_256[VEC_N_256_SIZE_64 >> 2];
    static uint64_t tmp4[VEC_N_256_SIZE_64];
    static uint64_t u[VEC_N_256_SIZE_64];
    static uint64_t v[VEC_N_256_SIZE_64];

    #ifdef __STDC_LIB_EXT1__
        memset_s(r2_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset_s(h_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset_s(s_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset_s(r1_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset_s(e_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
    #else
        memset(r2_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset(h_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset(s_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset(r1_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
        memset(e_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
    #endif

    // Create seed_expander from theta
    seedexpander_init(&seedexpander, theta, SEED_BYTES);

    static __m256i y_256[VEC_N_256_SIZE_64 >> 2];   
    static __m256i x_256[VEC_N_256_SIZE_64 >> 2];
    key_from_seed(pk_seed, sk_seed, h_256, s_256, x_256, y_256);



    // Generate r1, r2 and e
    vect_set_random_fixed_weight(&seedexpander, r1_256, PARAM_OMEGA_R);
    vect_set_random_fixed_weight(&seedexpander, r2_256, PARAM_OMEGA_R);
    vect_set_random_fixed_weight(&seedexpander, e_256, PARAM_OMEGA_E);

    // Compute u = r1 + r2.h
    vect_mul(tmp1_256, r2_256, h_256);
    vect_add(u, (uint64_t *) r1_256, (uint64_t *) tmp1_256, VEC_N_256_SIZE_64);

    // Compute v = m.G by encoding the message
    code_encode(v, m);
    vect_resize((uint64_t *) tmp2_256, PARAM_N, v, PARAM_N1N2);

    // Compute v = m.G + s.r2 + e
    vect_mul(tmp3_256, r2_256, s_256);
    vect_add(tmp4, (uint64_t *) e_256, (uint64_t *) tmp3_256, VEC_N_256_SIZE_64);
    vect_add((uint64_t *) tmp3_256, (uint64_t *) tmp2_256, tmp4, VEC_N_256_SIZE_64);
    vect_resize(v, PARAM_N1N2, (uint64_t *) tmp3_256, PARAM_N);

    
    printf("\n\nh: "); vect_print((uint64_t *) h_256, VEC_N_SIZE_BYTES);
    printf("\n\ns: "); vect_print((uint64_t *) s_256, VEC_N_SIZE_BYTES);
    printf("\n\nr1: "); vect_print((uint64_t *) r1_256, VEC_N_SIZE_BYTES);
    printf("\n\nr2: "); vect_print((uint64_t *) r2_256, VEC_N_SIZE_BYTES);
    printf("\n\ne: "); vect_print((uint64_t *) e_256, VEC_N_SIZE_BYTES);

    uint32_t locs[PARAM_OMEGA_R];
    vector_to_locations(r1_256, locs);
    printf("\n\nr1 locations: \n");
    for (uint32_t i = 0; i < PARAM_OMEGA_R; i++) printf("%u ", locs[i]);

    vector_to_locations(r2_256, locs);
    printf("\n\nr2 locations: \n");
    for (uint32_t i = 0; i < PARAM_OMEGA_R; i++) printf("%u ", locs[i]);

    vector_to_locations(e_256, locs);
    printf("\n\ne locations: \n");
    for (uint32_t i = 0; i < PARAM_OMEGA_E; i++) printf("%u ", locs[i]);

    printf("\n\ntmp3_256: "); vect_print((uint64_t *) tmp3_256, VEC_N_SIZE_BYTES);

    printf("\n\nu: "); vect_print(u, VEC_N_SIZE_BYTES);
    printf("\n\nv: "); vect_print(v, VEC_N1N2_SIZE_BYTES);
    
    printf("\n");
}