#include "api.h"
#include "hqc.h"
#include "parameters.h"
#include "parsing.h"
#include "shake_prng.h"
#include "shake_ds.h"
#include "gf2x.h"
#include "code.h"
#include "fips202.h"
#include "vector.h"
#include <stdint.h>
#include <string.h>
#include <immintrin.h>

void keygen(unsigned char* pk, unsigned char* sk, const uint8_t* sk_seed, const uint8_t* pk_seed) {
    seedexpander_state sk_seedexpander;
    seedexpander_state pk_seedexpander;
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

    seedexpander_init(&sk_seedexpander, sk_seed, SEED_BYTES);
    seedexpander_init(&pk_seedexpander, pk_seed, SEED_BYTES);

    // Compute secret key
    vect_set_random_fixed_weight(&sk_seedexpander, x_256, PARAM_OMEGA);
    vect_set_random_fixed_weight(&sk_seedexpander, y_256, PARAM_OMEGA);

    // Compute public key
    vect_set_random(&pk_seedexpander, (uint64_t *) h_256);
    vect_mul(tmp_256, y_256, h_256);
    vect_add(s, (uint64_t *) x_256, (uint64_t *) tmp_256, VEC_N_256_SIZE_64);

    // Parse keys to string
    hqc_public_key_to_string(pk, pk_seed, s);
    hqc_secret_key_to_string(sk, sk_seed, pk);
}

void encap(unsigned char *ct, unsigned char *ss, const unsigned char *pk, const unsigned char *msg) {
    uint8_t theta[SHAKE256_512_BYTES] = {0};
    uint64_t m[VEC_K_SIZE_64] = {0};
    static uint64_t u[VEC_N_256_SIZE_64] = {0};
    uint64_t v[VEC_N1N2_256_SIZE_64] = {0};
    uint8_t d[SHAKE256_512_BYTES] = {0};
    uint8_t mc[VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES] = {0};
    shake256incctx shake256state;

    // Computing m
    memcpy(m, msg, VEC_K_SIZE_BYTES);

    // Computing theta
    shake256_512_ds(&shake256state, theta, (uint8_t*) m, VEC_K_SIZE_BYTES, G_FCT_DOMAIN);

    // Encrypting m
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
}

int decap(unsigned char *ss, const unsigned char *ct, const unsigned char *sk) {
    uint8_t result;
    __m256i u_256[VEC_N_256_SIZE_64 >> 2] = {0};
    uint64_t v[VEC_N1N2_256_SIZE_64] = {0};
    uint8_t d[SHAKE256_512_BYTES] = {0};
    uint8_t pk[PUBLIC_KEY_BYTES] = {0};
    uint64_t m[VEC_K_SIZE_64] = {0};
    uint8_t theta[SHAKE256_512_BYTES] = {0};
    uint64_t u2[VEC_N_256_SIZE_64] = {0};
    uint64_t v2[VEC_N1N2_256_SIZE_64] = {0};
    uint8_t d2[SHAKE256_512_BYTES] = {0};
    uint8_t mc[VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES] = {0};
    shake256incctx shake256state;

    // Retrieving u, v and d from ciphertext
    hqc_ciphertext_from_string((uint64_t *) u_256, v , d, ct);

    // Retrieving pk from sk
    memcpy(pk, sk + SEED_BYTES, PUBLIC_KEY_BYTES);

    // Decryting
    hqc_pke_decrypt(m, u_256, v, sk);

    // Computing theta
    shake256_512_ds(&shake256state, theta, (uint8_t*) m, VEC_K_SIZE_BYTES, G_FCT_DOMAIN);

    // Encrypting m'
    hqc_pke_encrypt(u2, v2, m, theta, pk);

    // Computing d'
    shake256_512_ds(&shake256state, d2, (uint8_t *) m, VEC_K_SIZE_BYTES, H_FCT_DOMAIN);

    // Computing shared secret
    memcpy(mc, m, VEC_K_SIZE_BYTES);
    memcpy(mc + VEC_K_SIZE_BYTES, u_256, VEC_N_SIZE_BYTES);
    memcpy(mc + VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES, v, VEC_N1N2_SIZE_BYTES);
    shake256_512_ds(&shake256state, ss, mc, VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES, K_FCT_DOMAIN);

    // Abort if c != c' or d != d'
    result = vect_compare((uint8_t *) u_256, (uint8_t *) u2, VEC_N_SIZE_BYTES);
    result |= vect_compare((uint8_t *) v, (uint8_t *) v2, VEC_N1N2_SIZE_BYTES);
    result |= vect_compare(d, d2, SHAKE256_512_BYTES);

    result = (uint8_t) (-((int16_t) result) >> 15);

    for (size_t i = 0 ; i < SHARED_SECRET_BYTES ; i++) {
        ss[i] &= ~result;
    }

    return -(result & 1);
}