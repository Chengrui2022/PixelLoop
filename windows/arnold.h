#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int a;
    int b;
    int rounds;
} ArnoldKey;

void arnold_transform(const uint32_t *input, uint32_t *output, int size, ArnoldKey key, int decrypt);

#ifdef __cplusplus
}
#endif
