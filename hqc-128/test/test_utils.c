#include "test_utils.h"
#include "stdint.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "parameters.h"
#include "shake_prng.h"
#include <immintrin.h>
#include "vector.h"
#include "gf2x.h"

uint8_t ascii_to_hex(char c){
    if (c >= 48 && c <= 57) // 0-9
        return c - 48;
    else if (c >= 65 && c <= 70) // A-F
        return c - 55;
    else if (c >= 97 && c <= 102) // a-f
        return c - 87;
    else
        exit(-1);
}



void hex_str_to_bytes(uint8_t *bytes, char *hex_str, uint32_t len){
    for(uint32_t i = 0; i < len; i++){
        bytes[i] = ascii_to_hex(hex_str[2*i])*16 + ascii_to_hex(hex_str[2*i+1]);
    }
}



void print_hex(uint8_t *hex_array, uint32_t len){
    uint8_t *p = (uint8_t *) hex_array;
    for (uint32_t i = 0; i < len; i++, p++){
        printf("%02x", *p);
    }
}



void read_vect(char *filename, uint8_t *output, uint32_t outlen){
    char read_byte[8];
    char *curr = read_byte;
    char c;
    
    FILE *fp = fopen(filename, "r");
    if (!fp) {
        printf("file can't be opened \n");
        exit(-1);
    }

    while (outlen > 0){
        c = fgetc(fp);
        while(c < 48 || c > 57){
            c = fgetc(fp);
        }
        *curr = c - 48;
        curr++;
        if ((int) (curr - read_byte) == 8){
            curr = read_byte;
            outlen--;
            uint8_t acc = 0;
            for (uint8_t i = 0; i < 8; i++){
                acc += read_byte[i] * (1 << i);
            }
            *output = acc;
            output++;
        }
    }
    *(output-1) &= BITMASK(PARAM_N, 8);
    
    fclose(fp);
}



void write_vect(char *filename, uint8_t *input, uint32_t outlen, uint32_t newline_bytes){ 
    FILE *fp = fopen(filename, "w");
    if (!fp) {
        printf("file can't be opened \n");
        exit(-1);
    }

    uint32_t count_bytes = 0;
    while (outlen > 0){
        for (uint8_t i = 0; i < 8; i++){
            fputc(((*input >> i) % 2) + 48, fp);
        }
        count_bytes++;
        if (count_bytes == newline_bytes){
            fprintf(fp, "\n");
            count_bytes = 0;
        }
        input++;
        outlen--;
    }

    fclose(fp);
}



void vector_to_locations(__m256i *vec, uint32_t *locs){
    uint64_t *p = (uint64_t *) vec;
    uint64_t tmp;
    uint32_t count = 0;

    for (uint32_t i = 0; i < VEC_N_256_SIZE_64; i++){
        tmp = *p;
        if (tmp & 1){
            locs[count] = i * 64;
            count++;
        }
        for (uint32_t j = 1; j < 64; j++){
            if ((tmp >> 1) & 1){
                locs[count] = i * 64 + j;
                count++;
            }
            tmp = tmp >> 1;
        }
        p++;
    }
}



void key_from_seed(uint8_t *pk_seed, uint8_t *sk_seed, __m256i *h256, __m256i *s256, __m256i *x256, __m256i *y256){
    seedexpander_state sk_seedexpander;
    seedexpander_state pk_seedexpander;
    static __m256i h_256[VEC_N_256_SIZE_64 >> 2];
    static uint64_t s[VEC_N_256_SIZE_64];
    static __m256i y_256[VEC_N_256_SIZE_64 >> 2];   
    static __m256i x_256[VEC_N_256_SIZE_64 >> 2];
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


    // Create seed_expanders for public key and secret key
    seedexpander_init(&sk_seedexpander, sk_seed, SEED_BYTES);
    seedexpander_init(&pk_seedexpander, pk_seed, SEED_BYTES);

    // Compute secret key
    vect_set_random_fixed_weight(&sk_seedexpander, x_256, PARAM_OMEGA);
    vect_set_random_fixed_weight(&sk_seedexpander, y_256, PARAM_OMEGA);

    // Compute public key
    vect_set_random(&pk_seedexpander, (uint64_t *) h_256);
    vect_mul(tmp_256, y_256, h_256);
    vect_add(s, (uint64_t *) x_256, (uint64_t *) tmp_256, VEC_N_256_SIZE_64);

    memcpy(h256, h_256, VEC_N_256_SIZE_64 << 3);
    memcpy(s256, s, VEC_N_256_SIZE_64 << 3);
    memcpy(x256, x_256, VEC_N_256_SIZE_64 << 3);
    memcpy(y256, y_256, VEC_N_256_SIZE_64 << 3);
}
