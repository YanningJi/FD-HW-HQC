#ifndef HQC_H
#define HQC_H

/**
 * @file hqc.h
 * @brief Functions of the HQC_PKE IND_CPA scheme
 */

#include <stdint.h>
#include <immintrin.h>

void set_seed(unsigned char* pk_seed_i, unsigned char* sk_seed_i);
void hqc_pke_keygen(unsigned char* pk, unsigned char* sk);
void hqc_pke_encrypt(uint64_t *u, uint64_t *v, uint64_t *m, unsigned char *theta, const unsigned char *pk);
void hqc_pke_decrypt(uint64_t *m, const __m256i *u_256, const uint64_t *v, const uint8_t *sk);

#endif