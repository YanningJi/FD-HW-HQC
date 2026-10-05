#include <stdio.h>
#include "test_utils.h"
#include "parameters.h"
#include "gf.h"
#include <string.h>



int main() {
    char str[32 * 2];
    __m256i a, b, result;
    uint8_t vec1[32], vec2[32], vecresult[32];
    uint8_t *p;
    
    printf("Please input the first vector (32 bytes):\n");
    if(scanf("%s", str)){};
    hex_str_to_bytes(vec1, str, 32);

    printf("Please input the second vector (32 bytes):\n");
    if(scanf("%s", str)){};
    hex_str_to_bytes(vec2, str, 32);

    memcpy(&a, vec1, 32);
    memcpy(&b, vec2, 32);
    result = gf_mul_vect(a, b);
    memcpy(vecresult, &result, 32);

    printf("Result of gf_mul_vec:\n");
    print_hex(vecresult, 32);

    printf("\n");
}
