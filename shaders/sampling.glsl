#ifndef SAMPLING_GLSL
#define SAMPLING_GLSL

// Branchless orthonormal basis around a unit vector (Duff et al. 2017).
void orthonormalBasis(vec3 n, out vec3 t, out vec3 b) {
    float s = n.z >= 0.0 ? 1.0 : -1.0;
    float a = -1.0 / (s + n.z);
    float c = n.x * n.y * a;
    t = vec3(1.0 + s * n.x * n.x * a, s * c, -s * n.x);
    b = vec3(c, s + n.y * n.y * a, -n.y);
}

// Cosine-weighted direction around +Z; pdf = cos(theta) / pi.
vec3 sampleCosineHemisphere(vec2 u) {
    float r = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    return vec3(r * cos(phi), r * sin(phi), sqrt(max(0.0, 1.0 - u.x)));
}

// Uniform point on a triangle; pdf = 1 / area.
vec3 sampleTriangle(vec3 v0, vec3 v1, vec3 v2, vec2 u) {
    float su = sqrt(u.x);
    return v0 + (v1 - v0) * (su * (1.0 - u.y)) + (v2 - v0) * (su * u.y);
}

float powerHeuristic(float pdfA, float pdfB) {
    float a = pdfA * pdfA;
    float b = pdfB * pdfB;
    return a + b > 0.0 ? a / (a + b) : 0.0;
}

#endif
