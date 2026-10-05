#include <stdio.h>
#include "test_utils.h"
#include "hqc.h"
#include "api.h"
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
    uint64_t m_enc[VEC_K_SIZE_64];

    printf("Please input the public seed in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(pk_seed, str, SEED_BYTES);

    printf("Please input the secret seed in hex (%d bytes):\n", SEED_BYTES);
    if(scanf("%s", str)){};
    hex_str_to_bytes(sk_seed, str, SEED_BYTES);

    printf("Please input message in hex (%d bytes):\n", PARAM_K);
    if(scanf("%s", str)){};
    hex_str_to_bytes((uint8_t *) m_enc, str, PARAM_K);

    static __m256i h_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i s_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i y_256[VEC_N_256_SIZE_64 >> 2];
    static __m256i x_256[VEC_N_256_SIZE_64 >> 2];
    key_from_seed(pk_seed, sk_seed, h_256, s_256, x_256, y_256);

    uint8_t pk[PUBLIC_KEY_BYTES] = {0};
    uint8_t sk[SECRET_KEY_BYTES] = {0};
    uint8_t ct[VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES + SHAKE256_512_BYTES];
    uint8_t ss[64];
    hqc_public_key_to_string(pk, pk_seed, (uint64_t *) s_256);
    hqc_secret_key_to_string(sk, sk_seed, pk);


    uint8_t theta_enc[SHAKE256_512_BYTES] = {0};
    static uint64_t u_enc[VEC_N_256_SIZE_64] = {0};
    uint64_t v_enc[VEC_N1N2_256_SIZE_64] = {0};
    uint8_t d_enc[SHAKE256_512_BYTES] = {0};
    uint8_t mc_enc[VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES] = {0};
    shake256incctx shake256state_enc;
    
    // Computing theta
    shake256_512_ds(&shake256state_enc, theta_enc, (uint8_t*) m_enc, VEC_K_SIZE_BYTES, G_FCT_DOMAIN);

    // Encrypting m
    hqc_pke_encrypt(u_enc, v_enc, m_enc, theta_enc, pk);

    // Computing d
    shake256_512_ds(&shake256state_enc, d_enc, (uint8_t *) m_enc, VEC_K_SIZE_BYTES, H_FCT_DOMAIN);

    // Computing shared secret
    memcpy(mc_enc, m_enc, VEC_K_SIZE_BYTES);
    memcpy(mc_enc + VEC_K_SIZE_BYTES, u_enc, VEC_N_SIZE_BYTES);
    memcpy(mc_enc + VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES, v_enc, VEC_N1N2_SIZE_BYTES);
    shake256_512_ds(&shake256state_enc, ss, mc_enc, VEC_K_SIZE_BYTES + VEC_N_SIZE_BYTES + VEC_N1N2_SIZE_BYTES, K_FCT_DOMAIN);

    // Computing ciphertext
    hqc_ciphertext_to_string(ct, u_enc, v_enc, d_enc);



    uint8_t result;
    __m256i u_256[VEC_N_256_SIZE_64 >> 2] = {0};
    uint64_t v[VEC_N1N2_256_SIZE_64] = {0};
    uint8_t d[SHAKE256_512_BYTES] = {0};
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

    printf("\n\npk: "); for(int i = 0 ; i < PUBLIC_KEY_BYTES ; ++i) printf("%02x", pk[i]);
    printf("\n\nsk: "); for(int i = 0 ; i < SECRET_KEY_BYTES ; ++i) printf("%02x", sk[i]);
    printf("\n\nciphertext: "); for(int i = 0 ; i < CIPHERTEXT_BYTES ; ++i) printf("%02x", ct[i]);
    printf("\n\nm: "); vect_print(m, VEC_K_SIZE_BYTES);
    printf("\n\ntheta: "); for(int i = 0 ; i < SHAKE256_512_BYTES ; ++i) printf("%02x", theta[i]);
    printf("\n\n\n# Checking Ciphertext- Begin #");
    printf("\n\nu2: "); vect_print(u2, VEC_N_SIZE_BYTES);
    printf("\n\nv2: "); vect_print(v2, VEC_N1N2_SIZE_BYTES);
    printf("\n\nd2: "); for(int i = 0 ; i < SHAKE256_512_BYTES ; ++i) printf("%02x", d2[i]);
    if (result)
        printf("\n\nWhether new ciphertext is the same as before: No");
    else
        printf("\n\nWhether new ciphertext is the same as before: Yes");
    printf("\n\n# Checking Ciphertext - End #\n");

    printf("\n");
}