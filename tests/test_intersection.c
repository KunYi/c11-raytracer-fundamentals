#include "test_framework.h"
#include "vec3.h"
#include "ray.h"
#include "scene.h"
#include <math.h>

#define EPS 1e-9
#define EPS_F 1e-6

/* ── 共用材質（測試不關心材質值） ── */
static Material dummy_mat(void) {
    Material m = {{0,0,0},{0.5,0.5,0.5},{0.5,0.5,0.5},32.0};
    return m;
}

/* ══════════════════════════════════════
 *  三角形求交（Möller–Trumbore）
 * ══════════════════════════════════════ */

TEST(triangle, hit_center) {
    /* XY 平面上的三角形，光線從 +Z 垂直打向中心 */
    Object o = object_triangle(
        vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), dummy_mat());

    Ray ray = ray_make(vec3(0, 0.3, 5), vec3(0, 0, -1));
    HitRecord rec;
    int hit = object_intersect(&o, ray, 1e-4, 1e14, &rec);

    ASSERT_TRUE(hit);
    ASSERT_NEAR(rec.point.x, 0.0,  EPS_F);
    ASSERT_NEAR(rec.point.y, 0.3,  EPS_F);
    ASSERT_NEAR(rec.point.z, 0.0,  EPS_F);
}

TEST(triangle, miss_outside) {
    /* 光線打在三角形外側 */
    Object o = object_triangle(
        vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), dummy_mat());

    Ray ray = ray_make(vec3(5, 5, 5), vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_FALSE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
}

TEST(triangle, parallel_ray_misses) {
    /* 光線與三角形平面平行 → 必定不相交 */
    Object o = object_triangle(
        vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), dummy_mat());

    Ray ray = ray_make(vec3(0, 0, 1), vec3(1, 0, 0));  /* 沿 X 軸 */
    HitRecord rec;
    ASSERT_FALSE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
}

TEST(triangle, normal_faces_ray) {
    /* 法線應朝向光線來源側 */
    Object o = object_triangle(
        vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), dummy_mat());

    Ray ray = ray_make(vec3(0, 0.3, 5), vec3(0, 0, -1));
    HitRecord rec;
    object_intersect(&o, ray, 1e-4, 1e14, &rec);

    /* 法線應與光線方向點積 < 0（朝向光線來源） */
    ASSERT_TRUE(vec3_dot(rec.normal, ray.dir) < 0.0);
}

TEST(triangle, t_min_rejects_behind) {
    /* t_min 應排除起點後方的交點 */
    Object o = object_triangle(
        vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), dummy_mat());

    /* 光線起點在三角形後方，方向朝三角形，但我們把 t_min 設大 */
    Ray ray = ray_make(vec3(0, 0.3, -5), vec3(0, 0, 1));
    HitRecord rec;
    /* t 應該是正的（約 5），但被 t_max=1 排除 */
    ASSERT_FALSE(object_intersect(&o, ray, 1e-4, 1.0, &rec));
}

TEST(triangle, double_sided) {
    /* 雙面三角形：從背面打也要擊中，法線自動翻轉 */
    Object o = object_triangle(
        vec3(-1, 0, 0), vec3(1, 0, 0), vec3(0, 1, 0), dummy_mat());

    Ray ray_back = ray_make(vec3(0, 0.3, -5), vec3(0, 0, 1));
    HitRecord rec;
    ASSERT_TRUE(object_intersect(&o, ray_back, 1e-4, 1e14, &rec));
    ASSERT_TRUE(vec3_dot(rec.normal, ray_back.dir) < 0.0);
}

/* ══════════════════════════════════════
 *  球體求交
 * ══════════════════════════════════════ */

TEST(sphere, hit_front) {
    Object o = object_sphere(vec3(0, 0, 0), 1.0, dummy_mat());
    Ray ray = ray_make(vec3(0, 0, 5), vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_TRUE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
    /* 前交點在 z=1 處 */
    ASSERT_NEAR(rec.point.z, 1.0, EPS_F);
}

TEST(sphere, hit_t_is_distance) {
    /* dir 已正規化，t 就是距離 */
    Object o = object_sphere(vec3(0, 0, 0), 1.0, dummy_mat());
    Ray ray = ray_make(vec3(0, 0, 5), vec3(0, 0, -1));
    HitRecord rec;
    object_intersect(&o, ray, 1e-4, 1e14, &rec);
    /* 距離 = 5 - 1 = 4 */
    ASSERT_NEAR(rec.t, 4.0, EPS_F);
}

TEST(sphere, miss_outside) {
    Object o = object_sphere(vec3(0, 0, 0), 1.0, dummy_mat());
    Ray ray = ray_make(vec3(5, 5, 5), vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_FALSE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
}

TEST(sphere, normal_points_outward) {
    Object o = object_sphere(vec3(0, 0, 0), 1.0, dummy_mat());
    Ray ray = ray_make(vec3(0, 0, 5), vec3(0, 0, -1));
    HitRecord rec;
    object_intersect(&o, ray, 1e-4, 1e14, &rec);

    /* 法線 = (P - center) / r，從球心指向交點 */
    double len = vec3_len(rec.normal);
    ASSERT_NEAR(len, 1.0, EPS_F);
    /* 外部光線：法線朝光線來源 (+Z) */
    ASSERT_TRUE(rec.normal.z > 0.0);
}

TEST(sphere, ray_inside_hits_far_side) {
    /* 光線起點在球內部，應擊中遠端（t > 0 的那個根） */
    Object o = object_sphere(vec3(0, 0, 0), 2.0, dummy_mat());
    Ray ray = ray_make(vec3(0, 0, 0), vec3(0, 0, 1));
    HitRecord rec;
    ASSERT_TRUE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
    ASSERT_NEAR(rec.point.z, 2.0, EPS_F);
}

TEST(sphere, tangent_ray) {
    /* 切線光線（判別式 ≈ 0）：應擊中 */
    Object o = object_sphere(vec3(0, 0, 0), 1.0, dummy_mat());
    /* 沿 y=1 水平射入，恰好切球 */
    Ray ray = ray_make(vec3(-5, 1, 0), vec3(1, 0, 0));
    HitRecord rec;
    ASSERT_TRUE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
    ASSERT_NEAR(rec.point.y, 1.0, EPS_F);
}

/* ══════════════════════════════════════
 *  圓錐求交
 * ══════════════════════════════════════ */

TEST(cone, hit_side_from_front) {
    /* 圓錐：頂點 (0,2,0)，底面 y=0，半徑 1
     * 光線從正面（+Z）打向側面 */
    Object o = object_cone(
        vec3(0, 2, 0), 0.0, 1.0, dummy_mat(), dummy_mat());

    Ray ray = ray_make(vec3(0.4, 1.0, 5), vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_TRUE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
    /* 交點應在錐體高度範圍內 */
    ASSERT_TRUE(rec.point.y >= 0.0 && rec.point.y <= 2.0);
}

TEST(cone, miss_outside_radius) {
    /* 光線打在錐體底面半徑外 */
    Object o = object_cone(
        vec3(0, 2, 0), 0.0, 1.0, dummy_mat(), dummy_mat());

    Ray ray = ray_make(vec3(5, 1, 5), vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_FALSE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
}

TEST(cone, hit_base_disk) {
    /* 光線垂直從底面下方打進來 → 應擊中底面圓盤 */
    Object o = object_cone(
        vec3(0, 2, 0), 0.0, 1.0, dummy_mat(), dummy_mat());

    Ray ray = ray_make(vec3(0.3, -3, 0), vec3(0, 1, 0));
    HitRecord rec;
    ASSERT_TRUE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
    ASSERT_NEAR(rec.point.y, 0.0, EPS_F);
}

TEST(cone, miss_above_apex) {
    /* 光線打在頂點以上 → 應不相交 */
    Object o = object_cone(
        vec3(0, 2, 0), 0.0, 1.0, dummy_mat(), dummy_mat());

    Ray ray = ray_make(vec3(0, 5, 5), vec3(0, 0, -1));
    HitRecord rec;
    ASSERT_FALSE(object_intersect(&o, ray, 1e-4, 1e14, &rec));
}

TEST(cone, normal_faces_ray) {
    Object o = object_cone(
        vec3(0, 2, 0), 0.0, 1.0, dummy_mat(), dummy_mat());

    Ray ray = ray_make(vec3(0.4, 1.0, 5), vec3(0, 0, -1));
    HitRecord rec;
    object_intersect(&o, ray, 1e-4, 1e14, &rec);
    ASSERT_TRUE(vec3_dot(rec.normal, ray.dir) < 0.0);
}

/* ══════════════════════════════════════
 *  Bug 回歸測試：之前發現的問題
 * ══════════════════════════════════════ */

TEST(regression, sphere_self_intersection_bias) {
    /* 從球面交點再發出 Shadow Ray，t_min=1e-4 應避免自交 */
    Object o = object_sphere(vec3(0, 0, 0), 1.0, dummy_mat());
    Ray ray = ray_make(vec3(0, 0, 5), vec3(0, 0, -1));
    HitRecord rec;
    object_intersect(&o, ray, 1e-4, 1e14, &rec);

    /* 從交點出發向上射出 Shadow Ray */
    Ray shadow = ray_make(
        vec3_add(rec.point, vec3_scale(rec.normal, 1e-4)),
        vec3(0, 1, 0));

    HitRecord srec;
    /* 不應再打到自己（t_min 保護） */
    ASSERT_FALSE(object_intersect(&o, shadow, 1e-4, 1e14, &srec));
}

int main(void) {
    RUN_SUITE(triangle,
        hit_center, miss_outside, parallel_ray_misses,
        normal_faces_ray, t_min_rejects_behind, double_sided);
    RUN_SUITE(sphere,
        hit_front, hit_t_is_distance, miss_outside,
        normal_points_outward, ray_inside_hits_far_side, tangent_ray);
    RUN_SUITE(cone,
        hit_side_from_front, miss_outside_radius,
        hit_base_disk, miss_above_apex, normal_faces_ray);
    RUN_SUITE(regression,
        sphere_self_intersection_bias);
    return REPORT();
}
