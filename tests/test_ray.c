#include "test_framework.h"
#include "vec3.h"
#include "ray.h"
#include "camera.h"
#include <math.h>

#define EPS 1e-9
#define EPS_F 1e-6

/* ══════════════════════════════════════
 *  Ray
 * ══════════════════════════════════════ */

TEST(ray, make_normalizes_direction) {
    /* ray_make 應自動正規化方向 */
    Ray r = ray_make(vec3(0,0,0), vec3(3, 4, 0));
    ASSERT_NEAR(vec3_len(r.dir), 1.0, EPS_F);
}

TEST(ray, at_t0_equals_origin) {
    Ray r = ray_make(vec3(1, 2, 3), vec3(1, 0, 0));
    Vec3 p = ray_at(r, 0.0);
    ASSERT_VEC3_NEAR(p, 1, 2, 3, EPS);
}

TEST(ray, at_t1_unit_direction) {
    /* dir=(1,0,0)，t=5 → point=(6,2,3) */
    Ray r = ray_make(vec3(1, 2, 3), vec3(1, 0, 0));
    Vec3 p = ray_at(r, 5.0);
    ASSERT_VEC3_NEAR(p, 6, 2, 3, EPS_F);
}

TEST(ray, at_t_scales_correctly) {
    /* dir 正規化後長度=1，所以 ray_at(r, t) 距原點恰為 t */
    Ray r = ray_make(vec3(0,0,0), vec3(1, 1, 1));
    double t = 3.0;
    Vec3 p = ray_at(r, t);
    ASSERT_NEAR(vec3_len(p), t, EPS_F);
}

TEST(ray, direction_is_unit_after_make) {
    /* 任意非零方向，make 後必為單位向量 */
    Ray r = ray_make(vec3(5, -3, 2), vec3(2, -1, 7));
    ASSERT_NEAR(vec3_len(r.dir), 1.0, EPS_F);
}

/* ══════════════════════════════════════
 *  Camera
 * ══════════════════════════════════════ */

TEST(camera, center_ray_hits_lookat) {
    /* 相機在 (0,0,5) 看向原點，FOV=90°，aspect=1
     * 螢幕中心（s=0.5, t=0.5）射出的光線應指向原點方向 */
    Vec3 pos    = {0, 0, 5};
    Vec3 lookat = {0, 0, 0};
    Camera cam  = camera_init(pos, lookat, vec3(0,1,0), 90.0, 1.0);

    Ray r = camera_get_ray(&cam, 0.5, 0.5);

    /* 方向應接近 (0,0,-1)（從 +Z 朝向原點） */
    ASSERT_NEAR(r.dir.x, 0.0, EPS_F);
    ASSERT_NEAR(r.dir.y, 0.0, EPS_F);
    ASSERT_TRUE(r.dir.z < 0.0);  /* 朝 -Z */
}

TEST(camera, ray_origin_is_position) {
    Vec3 pos    = {3, 2, 1};
    Vec3 lookat = {0, 0, 0};
    Camera cam  = camera_init(pos, lookat, vec3(0,1,0), 45.0, 1.0);

    Ray r = camera_get_ray(&cam, 0.5, 0.5);
    ASSERT_VEC3_NEAR(r.origin, 3, 2, 1, EPS_F);
}

TEST(camera, ray_direction_is_unit) {
    Camera cam = camera_init(
        vec3(0, 1, 5), vec3(0, 0, 0), vec3(0, 1, 0), 60.0, 16.0/9.0);

    /* 測試多個像素位置，方向都應為單位向量 */
    double ss[] = {0.0, 0.25, 0.5, 0.75, 1.0};
    for (int i = 0; i < 5; ++i) {
        for (int j = 0; j < 5; ++j) {
            Ray r = camera_get_ray(&cam, ss[i], ss[j]);
            ASSERT_NEAR(vec3_len(r.dir), 1.0, EPS_F);
        }
    }
}

TEST(camera, fov90_half_width_at_focal_length_1) {
    /* FOV=90°，aspect=1，focal_length=1
     * 螢幕半寬 = tan(45°) = 1
     * s=0（最左）→ u 方向為 -1；s=1（最右）→ u 方向為 +1 */
    Camera cam = camera_init(
        vec3(0, 0, 5), vec3(0, 0, 0), vec3(0,1,0), 90.0, 1.0);

    Ray left  = camera_get_ray(&cam, 0.0, 0.5);  /* 最左邊，垂直居中 */
    Ray right = camera_get_ray(&cam, 1.0, 0.5);  /* 最右邊，垂直居中 */

    /* 左邊光線 x 分量應為負，右邊應為正 */
    ASSERT_TRUE(left.dir.x  < 0.0);
    ASSERT_TRUE(right.dir.x > 0.0);
    /* 兩者 x 分量大小相等（對稱） */
    ASSERT_NEAR(left.dir.x, -right.dir.x, EPS_F);
}

TEST(camera, lookat_changes_direction) {
    /* 相機朝不同方向看，中心光線方向應不同 */
    Vec3 pos = {0, 0, 5};
    Camera cam1 = camera_init(pos, vec3( 0, 0, 0), vec3(0,1,0), 60.0, 1.0);
    Camera cam2 = camera_init(pos, vec3( 1, 0, 0), vec3(0,1,0), 60.0, 1.0);

    Ray r1 = camera_get_ray(&cam1, 0.5, 0.5);
    Ray r2 = camera_get_ray(&cam2, 0.5, 0.5);

    /* 兩個中心光線方向應不同 */
    ASSERT_TRUE(fabs(r1.dir.x - r2.dir.x) > EPS_F ||
                fabs(r1.dir.z - r2.dir.z) > EPS_F);
}

int main(void) {
    RUN_SUITE(ray,
        make_normalizes_direction, at_t0_equals_origin,
        at_t1_unit_direction, at_t_scales_correctly,
        direction_is_unit_after_make);
    RUN_SUITE(camera,
        center_ray_hits_lookat, ray_origin_is_position,
        ray_direction_is_unit, fov90_half_width_at_focal_length_1,
        lookat_changes_direction);
    return REPORT();
}
