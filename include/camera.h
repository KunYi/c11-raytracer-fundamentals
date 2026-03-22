#ifndef CAMERA_H
#define CAMERA_H

#include <math.h>
#include "vec3.h"
#include "ray.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/*
 * ── Pinhole Camera ──
 *
 *  position  : 相機世界座標
 *  lookat    : 相機看向的目標點
 *  up        : 世界上方向（通常為 (0,1,0)）
 *  fov_deg   : 垂直視角（度）
 *  aspect    : 寬高比 (width / height)
 *
 * 建立右手座標系 (u, v, w)：
 *   w = normalize(position - lookat)   ← 相機「後方」
 *   u = normalize(cross(up, w))        ← 相機「右方」
 *   v = cross(w, u)                    ← 相機「上方」
 *
 * 虛擬螢幕（focal_length = 1）放在 -w 方向，
 * 半高 = tan(fov/2)，半寬 = half_h * aspect
 */
typedef struct {
    Vec3   position;
    Vec3   lower_left;   /* 螢幕左下角世界座標 */
    Vec3   horizontal;   /* 螢幕水平向量 */
    Vec3   vertical;     /* 螢幕垂直向量 */
} Camera;

/*
 * camera_init
 *   position : 相機位置
 *   lookat   : 目標點
 *   up       : 上方向
 *   fov_deg  : 垂直 FOV（度）
 *   aspect   : 寬/高
 */
static inline Camera camera_init(Vec3 position, Vec3 lookat,
                                  Vec3 up, double fov_deg, double aspect)
{
    double theta   = fov_deg * M_PI / 180.0;
    double half_h  = tan(theta / 2.0);
    double half_w  = aspect * half_h;

    Vec3 w = vec3_normalize(vec3_sub(position, lookat));
    Vec3 u = vec3_normalize(vec3_cross(up, w));
    Vec3 v = vec3_cross(w, u);

    /* 螢幕在 focal_length = 1 處 */
    Vec3 center = vec3_sub(position, w);  /* position - 1*w */

    Vec3 lower_left = vec3_sub(
        vec3_sub(center, vec3_scale(u, half_w)),
        vec3_scale(v, half_h)
    );

    Camera cam;
    cam.position   = position;
    cam.lower_left = lower_left;
    cam.horizontal = vec3_scale(u, 2.0 * half_w);
    cam.vertical   = vec3_scale(v, 2.0 * half_h);
    return cam;
}

/*
 * camera_get_ray
 *   s, t : 歸一化螢幕座標 [0,1] × [0,1]（左下角為原點）
 */
static inline Ray camera_get_ray(const Camera *cam, double s, double t) {
    Vec3 target = vec3_add(
        cam->lower_left,
        vec3_add(vec3_scale(cam->horizontal, s),
                 vec3_scale(cam->vertical,   t))
    );
    Vec3 dir = vec3_sub(target, cam->position);
    return ray_make(cam->position, dir);
}

#endif /* CAMERA_H */
