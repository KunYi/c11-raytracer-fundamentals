#ifndef RAY_H
#define RAY_H

#include "vec3.h"

/* ── 光線：origin + t * direction ── */
typedef struct {
    Vec3 origin;
    Vec3 dir;      /* 已 normalize */
} Ray;

static inline Ray ray_make(Vec3 origin, Vec3 dir) {
    return (Ray){origin, vec3_normalize(dir)};
}

/* 取光線上 t 處的點 */
static inline Vec3 ray_at(Ray r, double t) {
    return vec3_add(r.origin, vec3_scale(r.dir, t));
}

#endif /* RAY_H */
