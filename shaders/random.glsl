#ifndef RANDOM_GLSL
#define RANDOM_GLSL

// PCG-RXS-M-XS 32-bit (O'Neill), used both as a hash for seeding and as the generator.
uint rngState;

uint pcgHash(uint v) {
    uint state = v * 747796405u + 2891336453u;
    uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
    return (word >> 22u) ^ word;
}

// Decorrelated stream per (pixel, sample index).
void seedRng(uvec2 pixel, uint sampleIndex) {
    rngState = pcgHash(pixel.x + pcgHash(pixel.y + pcgHash(sampleIndex)));
}

// Uniform in [0, 1). Uses the top 24 bits so the result is exactly representable and never 1.0
// (the old version divided a 32-bit word by 2^32, which rounds to 1.0 in float).
float rand() {
    rngState = rngState * 747796405u + 2891336453u;
    uint word = ((rngState >> ((rngState >> 28u) + 4u)) ^ rngState) * 277803737u;
    word = (word >> 22u) ^ word;
    return float(word >> 8u) * (1.0 / 16777216.0);
}

vec2 rand2() {
    return vec2(rand(), rand());
}

#endif
