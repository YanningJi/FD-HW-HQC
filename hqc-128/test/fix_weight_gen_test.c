#include <stdio.h>
#include "test_utils.h"
#include "shake_prng.h"
#include "parameters.h"
#include "vector.h"



int main(){
    seedexpander_state seedexpander_s;
    uint8_t seed[SEED_BYTES] = {0};

    char str[SEED_BYTES * 2];
    uint16_t weight;

    printf("Please input the weight:\n");
    if(scanf("%hu", &weight)){};

    printf("Please input the seed in hex:\n");
    if(scanf("%s", str)){};
    hex_str_to_bytes(seed, str, SEED_BYTES);

    seedexpander_init(&seedexpander_s, seed, SEED_BYTES); // append 2 as the domain separator at the end

    size_t random_bytes_size = 3 * weight;
    uint8_t rand_bytes[3 * PARAM_OMEGA_R] = {0};
    uint32_t random_data = 0;
    uint32_t tmp[PARAM_OMEGA_R] = {0};
    uint8_t exist = 0;
    size_t j = 0;

    seedexpander(&seedexpander_s, rand_bytes, random_bytes_size);

    for (uint32_t i = 0 ; i < weight ; ++i) {
        exist = 0;
        do {
            if (j == random_bytes_size) {
                seedexpander(&seedexpander_s, rand_bytes, random_bytes_size);
                j = 0;
            }

            random_data  = ((uint32_t) rand_bytes[j++]) << 16;
            random_data |= ((uint32_t) rand_bytes[j++]) << 8;
            random_data |= rand_bytes[j++];

        } while (random_data >= UTILS_REJECTION_THRESHOLD);

        random_data = random_data % PARAM_N;

        for (uint32_t k = 0 ; k < i ; k++) {
            if (tmp[k] == random_data) {
                exist = 1;
            }
        }

        if (exist == 1) {
            i--;
        } else {
            tmp[i] = random_data;
        }
    }

    printf("Locations are:\n");

    for (int i = 0; i < weight; i++){
        printf("%d\n", tmp[i]);
    }


    printf("\n");
}

