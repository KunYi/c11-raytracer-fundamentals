#ifndef VEC3_H
#define VEC3_H

#include <math.h>

/* ── 三維向量 ── */
typedef struct {
    double x, y, z;
} Vec3;

/* 基本運算 */
static inline Vec3 vec3(double x, double y, double z) {
    return (Vec3){x, y, z};
}

static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}

static inline Vec3 vec3_scale(Vec3 v, double s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}

static inline Vec3 vec3_mul(Vec3 a, Vec3 b) {          /* 分量相乘（用於顏色混合） */
    return (Vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}

static inline double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,
        a.z * b.x - a.x * b.z,
        a.x * b.y - a.y * b.x
    };
}

static inline double vec3_len(Vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline Vec3 vec3_normalize(Vec3 v) {
    double len = vec3_len(v);
    if (len < 1e-12) return (Vec3){0, 0, 0};
    return vec3_scale(v, 1.0 / len);
}

static inline Vec3 vec3_negate(Vec3 v) {
    return (Vec3){-v.x, -v.y, -v.z};
}

static inline Vec3 vec3_reflect(Vec3 d, Vec3 n) {
    /* r = d - 2*(d·n)*n  （d 為入射方向，n 為法線，均已 normalize） */
    return vec3_sub(d, vec3_scale(n, 2.0 * vec3_dot(d, n)));
}

static inline Vec3 vec3_clamp(Vec3 v, double lo, double hi) {
    double cx = v.x < lo ? lo : (v.x > hi ? hi : v.x);
    double cy = v.y < lo ? lo : (v.y > hi ? hi : v.y);
    double cz = v.z < lo ? lo : (v.z > hi ? hi : v.z);
    return (Vec3){cx, cy, cz};
}

#endif /* VEC3_H */
