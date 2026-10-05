#include <stdio.h>
#include "test_utils.h"
#include "shake_prng.h"
#include "parameters.h"
#include "gf2x.h"



int main(){
    __m256i h_256[VEC_N_256_SIZE_64 >> 2] = {0};
    __m256i y_256[VEC_N_256_SIZE_64 >> 2] = {0};   
    __m256i tmp_256[VEC_N_256_SIZE_64 >> 2];
    uint8_t *p = NULL;
    char vec_filename[100];
    uint32_t outlen = VEC_N_SIZE_BYTES;

    printf("Please input the filename which contains the first vector in binary (vector length = %d bytes):\n", VEC_N_SIZE_BYTES);
    if(scanf("%s", vec_filename)) {};
    read_vect(vec_filename, (uint8_t *) h_256, outlen);

    printf("Please input the filename which contains the second vector in binary (vector length = %d bytes):\n", VEC_N_SIZE_BYTES);
    if(scanf("%s", vec_filename)) {};
    read_vect(vec_filename, (uint8_t *) y_256, outlen);

    vect_mul(tmp_256, y_256, h_256);

    printf("\nResult is:\n");
    p = (uint8_t *) tmp_256;
    uint8_t count_bytes = 0;
    for(uint32_t i = outlen; i > 0; i--, p++){
        for (uint8_t j = 0; j < 8; j++){
            printf("%d", (*p >> j) % 2);
        }
        printf(" ");
        count_bytes++;
        if(count_bytes == 32){
            printf("\n");
            count_bytes = 0;
        }
    }
    
    printf("\nPlease input the filename which stores the result\n");
    if(scanf("%s", vec_filename)) {};
    write_vect(vec_filename, (uint8_t *) tmp_256, outlen, 32);
}

