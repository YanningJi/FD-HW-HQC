#include "code.h"
#include "reed_muller.h"
#include "reed_solomon.h"
#include "parameters.h"
#include <stdint.h>
#include <string.h>

int main(const uint64_t *m)
{
    uint64_t tmp[VEC_N1_SIZE_64] = {0};
    reed_solomon_encode(tmp, m);
    return 0;
}