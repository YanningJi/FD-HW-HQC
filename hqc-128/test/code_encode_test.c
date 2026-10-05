#include <stdio.h>
#include "test_utils.h"
#include "parameters.h"
#include "code.h"



int main() {
    char str[PARAM_K * 2];
    uint64_t msg[VEC_K_SIZE_64];
    uint64_t cdw[VEC_N1N2_SIZE_64];
    
    printf("Please input the message in hex (%d bytes):\n", PARAM_K);
    if(scanf("%s", str)){};
    hex_str_to_bytes((uint8_t *) msg, str, PARAM_K);

    code_encode(cdw, msg);

    printf("\n\nCiphertext is (%d bytes):\n", PARAM_N1N2 / 8);
    print_hex((uint8_t *) cdw, PARAM_N1N2 / 8);

    printf("\n");
}