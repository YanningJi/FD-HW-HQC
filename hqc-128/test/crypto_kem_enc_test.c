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
#include "shake_ds.h"



int main() {
    char str[(SEED_BYTES > PARAM_K ? SEED_BYTES : PARAM_K) * 2];
    uint8_t pk_seed[SEED_BYTES] = {0};
    uint8_t sk_seed[SEED_BYTES] = {0};
    uint64_t m[VEC_K_SIZE_64];

    printf("Please input the public seed in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(pk_seed, str, SEED_BYTES);

    printf("Please input the secret seed in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(sk_seed, str, SEED_BYTES);

    printf("Please input message in hex (%d bytes):\n", PARAM_K);
    if(scanf("%s", str)){};
    hex_str_to_bytes((uint8_t *) m, str, PARAM_K);

    static __m256i h_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i s_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i y_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i x_256[VEC_N_256_SIZE_64 >> 2];
    key_from_seed(pk_seed, sk_seed, h_256, s_256, x_256, y_256);



    uint8_t theta[SHAKE256_512_BYTES] = {0};
    static uint64_t u[VEC_N_256_SIZE_64] = {0};
    uint64_t v[VEC_N1N2_256_SIZE_64] = {0};
    uint8_t d[SHAKE256_512_BYTES] = {0};
    uint8_t mc[VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES] = {0};
    uint8_t ss[512/8];
    uint8_t ct[VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES + SHAKE256_512_BYTES];
    shake256incctx shake256state;

    // Computing m
    // vect_set_random_from_prng(m);

    // Computing theta
    shake256_512_ds(&shake256state, theta, (uint8_t*) m, VEC_K_SIZE_BYTES, G_FCT_DOMAIN);

    // Encrypting m
    uint8_t pk[SEED_BYTES + VEC_N_SIZE_BYTES];
    hqc_public_key_to_string(pk, pk_seed, (uint64_t *) s_256);
    hqc_pke_encrypt(u, v, m, theta, pk);

    // Computing d
    shake256_512_ds(&shake256state, d, (uint8_t *) m, VEC_K_SIZE_BYTES, H_FCT_DOMAIN);

    // Computing shared secret
    memcpy(mc, m, VEC_K_SIZE_BYTES);
    memcpy(mc + VEC_K_SIZE_BYTES, u, VEC_N_SIZE_BYTES);
    memcpy(mc + VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES, v, VEC_N1N2_SIZE_BYTES);
    shake256_512_ds(&shake256state, ss, mc, VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES, K_FCT_DOMAIN);

    // Computing ciphertext
    hqc_ciphertext_to_string(ct, u, v, d);

//    printf("\n\npk: "); for(int i = 0 ; i < PUBLIC_KEY_BYTES ; ++i) printf("%02x", pk[i]);
//    printf("\n\nm: "); vect_print(m, VEC_K_SIZE_BYTES);
    printf("\n\ntheta: "); for(int i = 0 ; i < SHAKE256_512_BYTES ; ++i) printf("%02x", theta[i]);
    printf("\n\nd: "); for(int i = 0 ; i < SHAKE256_512_BYTES ; ++i) printf("%02x", d[i]);
    printf("\n\nciphertext: "); for(int i = 0 ; i < CIPHERTEXT_BYTES ; ++i) printf("%02x", ct[i]);
    printf("\n\nshared secret: "); for(int i = 0 ; i < SHARED_SECRET_BYTES ; ++i) printf("%02x", ss[i]);

    printf("\n");
}