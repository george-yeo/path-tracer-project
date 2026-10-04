// Metal/roughness BSDF: Lambert diffuse + GGX microfacet specular (height-correlation ignored,
// separable Smith G). Sampling picks one lobe at random and returns the pdf of the *combined*
// one-sample mixture, which is what both the throughput update and the MIS weights need.
#ifndef BSDF_GLSL
#define BSDF_GLSL

const float MIN_ROUGHNESS = 0.03; // alpha >= 9e-4: near-mirror without a separate delta path

struct Bsdf {
    vec3 albedo;
    vec3 f0;
    float alpha;
    float metallic;
    float specularProbability;
};

float luminance(vec3 c) {
    return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 fresnelSchlick(float cosTheta, vec3 f0) {
    return f0 + (1.0 - f0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float ggxD(float NdotH, float a2) {
    float d = NdotH * NdotH * (a2 - 1.0) + 1.0;
    return a2 / (PI * d * d);
}

float smithG1(float NdotX, float a2) {
    return 2.0 * NdotX / (NdotX + sqrt(a2 + (1.0 - a2) * NdotX * NdotX));
}

Bsdf makeBsdf(Material m, float NdotV) {
    Bsdf b;
    b.albedo = m.color;
    b.metallic = m.metallic;
    b.f0 = mix(vec3(0.04), m.color, m.metallic);
    float roughness = clamp(m.roughness, MIN_ROUGHNESS, 1.0);
    b.alpha = roughness * roughness;

    // Lobe selection proportional to an estimate of each lobe's reflectance. Clamped away from
    // 0/1 when both lobes exist so neither is ever starved of samples.
    float spec = luminance(fresnelSchlick(NdotV, b.f0));
    float diff = luminance(m.color) * (1.0 - m.metallic) * (1.0 - spec);
    b.specularProbability = diff <= 0.0 ? 1.0 : clamp(spec / (spec + diff), 0.1, 0.9);
    return b;
}

// BSDF value f(V, L) (without the cosine term). V and L point away from the surface.
vec3 evalBsdf(Bsdf b, vec3 N, vec3 V, vec3 L) {
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if (NdotL <= 0.0 || NdotV <= 0.0) {
        return vec3(0.0);
    }
    vec3 H = normalize(V + L);
    float NdotH = max(dot(N, H), 0.0);
    float VdotH = max(dot(V, H), 0.0);
    float a2 = b.alpha * b.alpha;

    vec3 F = fresnelSchlick(VdotH, b.f0);
    vec3 specular = F * (ggxD(NdotH, a2) * smithG1(NdotV, a2) * smithG1(NdotL, a2) /
                         (4.0 * NdotL * NdotV));
    vec3 diffuse = (1.0 - F) * (1.0 - b.metallic) * b.albedo / PI;
    return diffuse + specular;
}

// Solid-angle pdf of sampleBsdf() producing L.
// Specular lobe: visible-normal sampling, pdf_L = D_V(H) / (4 V.H) = G1(V) D(H) / (4 N.V).
float pdfBsdf(Bsdf b, vec3 N, vec3 V, vec3 L) {
    float NdotL = dot(N, L);
    float NdotV = dot(N, V);
    if (NdotL <= 0.0 || NdotV <= 0.0) {
        return 0.0;
    }
    vec3 H = normalize(V + L);
    float a2 = b.alpha * b.alpha;
    float specularPdf = ggxD(max(dot(N, H), 0.0), a2) * smithG1(NdotV, a2) / (4.0 * NdotV);
    float diffusePdf = NdotL / PI;
    return mix(diffusePdf, specularPdf, b.specularProbability);
}

// Visible normal sampling of the GGX distribution (Heitz 2018), in the local frame (+Z = N).
vec3 sampleGgxVndf(vec3 Ve, float alpha, vec2 u) {
    vec3 Vh = normalize(vec3(alpha * Ve.x, alpha * Ve.y, Ve.z));
    float lensq = Vh.x * Vh.x + Vh.y * Vh.y;
    vec3 T1 = lensq > 0.0 ? vec3(-Vh.y, Vh.x, 0.0) * inversesqrt(lensq) : vec3(1.0, 0.0, 0.0);
    vec3 T2 = cross(Vh, T1);
    float r = sqrt(u.x);
    float phi = 2.0 * PI * u.y;
    float t1 = r * cos(phi);
    float t2 = r * sin(phi);
    float s = 0.5 * (1.0 + Vh.z);
    t2 = (1.0 - s) * sqrt(max(0.0, 1.0 - t1 * t1)) + s * t2;
    vec3 Nh = t1 * T1 + t2 * T2 + sqrt(max(0.0, 1.0 - t1 * t1 - t2 * t2)) * Vh;
    return normalize(vec3(alpha * Nh.x, alpha * Nh.y, max(0.0, Nh.z)));
}

// Returns false if the sampled direction is below the surface (path is absorbed).
bool sampleBsdf(Bsdf b, vec3 N, vec3 V, vec3 u, out vec3 L) {
    vec3 T;
    vec3 B;
    orthonormalBasis(N, T, B);
    if (u.x < b.specularProbability) {
        vec3 Vlocal = vec3(dot(V, T), dot(V, B), dot(V, N));
        vec3 H = sampleGgxVndf(Vlocal, b.alpha, u.yz);
        L = reflect(-V, H.x * T + H.y * B + H.z * N);
    } else {
        vec3 d = sampleCosineHemisphere(u.yz);
        L = d.x * T + d.y * B + d.z * N;
    }
    return dot(N, L) > 0.0;
}

#endif
