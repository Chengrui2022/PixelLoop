#include "arnold.h"

typedef struct {
    int p, q, r, s;
} Matrix;

static int mod(int value, int size) {
    const int remainder = value % size;
    return remainder < 0 ? remainder + size : remainder;
}

static Matrix multiply(Matrix l, Matrix r, int size) {
    return (Matrix){
        mod(l.p * r.p + l.q * r.r, size),
        mod(l.p * r.q + l.q * r.s, size),
        mod(l.r * r.p + l.s * r.r, size),
        mod(l.r * r.q + l.s * r.s, size)
    };
}

static Matrix power(Matrix base, int rounds, int size) {
    Matrix result = {1, 0, 0, 1};
    while (rounds > 0) {
        if (rounds % 2 == 1) {
            result = multiply(result, base, size);
        }
        base = multiply(base, base, size);
        rounds /= 2;
    }
    return result;
}

void arnold_transform(const uint32_t *input, uint32_t *output, int size, ArnoldKey key, int decrypt) {
    const int diagonal = 1 + key.a * key.b;
    const Matrix base = decrypt
        ? (Matrix){diagonal, -key.a, -key.b, 1}
        : (Matrix){1, key.a, key.b, diagonal};
    const Matrix map = power(base, key.rounds, size);
    for (int row = 0; row < size; ++row) {
        for (int col = 0; col < size; ++col) {
            const int new_row = mod(map.p * row + map.q * col, size);
            const int new_col = mod(map.r * row + map.s * col, size);
            output[new_row * size + new_col] = input[row * size + col];
        }
    }
}
