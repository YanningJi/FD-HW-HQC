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
    uint64_t m_decoded[VEC_K_SIZE_64];

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



    static __m256i x_256[VEC_N_256_SIZE_64 >> 2] = {0};
    static __m256i y_256[VEC_N_256_SIZE_64 >> 2] = {0};
    static __m256i h_256[VEC_N_256_SIZE_64 >> 2] = {0};
    static __m256i s_256[VEC_N_256_SIZE_64 >> 2] = {0};
    uint8_t pk[PUBLIC_KEY_BYTES] = {0};
    static uint64_t v[VEC_N_256_SIZE_64] = {0};
    static uint64_t u[VEC_N_256_SIZE_64] = {0};
    static uint64_t tmp1[VEC_N_256_SIZE_64] = {0};
    static uint64_t tmp2[VEC_N_256_SIZE_64] = {0};
    static __m256i tmp3_256[VEC_N_256_SIZE_64 >> 2];

    #ifdef __STDC_LIB_EXT1__
        memset_s(y_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
    #else
        memset(y_256, 0, (VEC_N_256_SIZE_64 >> 2) * sizeof(__m256i));
    #endif

    // Create seed_expanders for public key and secret key
    key_from_seed(pk_seed, sk_seed, h_256, s_256, x_256, y_256);
    hqc_public_key_to_string(pk, pk_seed, (uint64_t *) s_256);

    hqc_pke_encrypt(u, (uint64_t *) v, m, theta, pk);


    // Compute v - u.y
    vect_resize(tmp1, PARAM_N, v, PARAM_N1N2);
    vect_mul(tmp3_256, y_256, (__m256i *) u);
    vect_add(tmp2, tmp1, (uint64_t *) tmp3_256, VEC_N_256_SIZE_64);

    printf("\n\nu: "); vect_print(u, VEC_N_SIZE_BYTES);
    printf("\n\nv: "); vect_print(v, VEC_N1N2_SIZE_BYTES);
    printf("\n\ny: "); vect_print((uint64_t *) y_256, VEC_N_SIZE_BYTES);
    printf("\n\nv - u.y: "); vect_print(tmp2, VEC_N_SIZE_BYTES);

    // Compute m by decoding v - u.y
    code_decode(m_decoded, tmp2);
    printf("\n\ndecoded message: ");
    print_hex((uint8_t *) m_decoded, PARAM_K);
    printf("\n");
}