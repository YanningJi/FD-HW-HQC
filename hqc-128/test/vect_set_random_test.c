#include <stdio.h>
#include "test_utils.h"
#include "shake_prng.h"
#include "parameters.h"
#include "vector.h"



int main(){
    seedexpander_state seedexpander;
    uint8_t seed[SEED_BYTES] = {0};

    char str[SEED_BYTES * 2];

    __m256i h_256[VEC_N_256_SIZE_64 >> 2]; // use PARAM_N_MULT as N

    printf("Please input the seed in hex:\n");
    if(scanf("%s", str)) {};
    hex_str_to_bytes(seed, str, SEED_BYTES);


    seedexpander_init(&seedexpander, seed, SEED_BYTES); // append 2 as the domain separator at the end
    vect_set_random(&seedexpander, (uint64_t *) h_256); // generates VEC_N_SIZE_BYTES bytes, and mask the most significant bits


    printf("Random vector is:\n");
    print_hex((uint8_t *) h_256, VEC_N_SIZE_BYTES);
    printf("\n");
}

