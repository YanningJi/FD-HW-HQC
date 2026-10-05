#ifndef TEST_UTILS
#define TEST_UTILS

#include "stdint.h"
#include <immintrin.h>

uint8_t ascii_to_hex(char c);
void hex_str_to_bytes(uint8_t *bytes, char *hex_str, uint32_t len);
void print_hex(uint8_t *hex_array, uint32_t len);
void read_vect(char *filename, uint8_t *output, uint32_t outlen);
void write_vect(char *filename, uint8_t *input, uint32_t outlen, uint32_t newline_bytes);
void vector_to_locations(__m256i *vec, uint32_t *locs);
void key_from_seed(uint8_t *pk_seed, uint8_t *sk_seed, __m256i *h256, __m256i *s256, __m256i *x256, __m256i *y256);

#endif