'use strict';

const Arnold = (() => {
  const mod = (value, size) => ((value % size) + size) % size;

  function matrix(a, b, direction = 'encrypt') {
    if (direction === 'decrypt') {
      return [1 + a * b, -a, -b, 1];
    }
    return [1, a, b, 1 + a * b];
  }

  function multiply(left, right, size) {
    const [a, b, c, d] = left;
    const [e, f, g, h] = right;
    return [
      mod(a * e + b * g, size), mod(a * f + b * h, size),
      mod(c * e + d * g, size), mod(c * f + d * h, size)
    ];
  }

  function power(base, rounds, size) {
    let result = [1, 0, 0, 1];
    while (rounds > 0) {
      if (rounds % 2 === 1) {
        result = multiply(result, base, size);
      }
      base = multiply(base, base, size);
      rounds = Math.floor(rounds / 2);
    }
    return result;
  }

  function transform(pixels, size, { a, b, rounds }, direction) {
    const [p, q, r, s] = power(matrix(a, b, direction), rounds, size);
    const output = new Uint8ClampedArray(pixels.length);

    for (let row = 0; row < size; row += 1) {
      for (let col = 0; col < size; col += 1) {
        const newRow = mod(p * row + q * col, size);
        const newCol = mod(r * row + s * col, size);
        const from = 4 * (row * size + col);
        const to = 4 * (newRow * size + newCol);
        output[to] = pixels[from];
        output[to + 1] = pixels[from + 1];
        output[to + 2] = pixels[from + 2];
        output[to + 3] = pixels[from + 3];
      }
    }
    return output;
  }

  return { matrix, transform };
})();
