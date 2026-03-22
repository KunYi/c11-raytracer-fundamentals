#include "test_framework.h"
#include "vec3.h"
#include <math.h>

#define EPS 1e-9   /* 精確運算的容忍誤差 */
#define EPS_F 1e-6 /* 涉及 sqrt/trig 的容忍誤差 */

/* ══════════════════════════════════════
 *  基本運算
 * ══════════════════════════════════════ */

TEST(vec3, add_simple) {
    Vec3 a = {1, 2, 3};
    Vec3 b = {4, 5, 6};
    Vec3 r = vec3_add(a, b);
    ASSERT_VEC3_NEAR(r, 5, 7, 9, EPS);
}

TEST(vec3, add_negative) {
    Vec3 a = { 1.5, -2.0, 0.0};
    Vec3 b = {-1.5,  2.0, 3.0};
    Vec3 r = vec3_add(a, b);
    ASSERT_VEC3_NEAR(r, 0, 0, 3, EPS);
}

TEST(vec3, sub_simple) {
    Vec3 a = {5, 7, 9};
    Vec3 b = {1, 2, 3};
    Vec3 r = vec3_sub(a, b);
    ASSERT_VEC3_NEAR(r, 4, 5, 6, EPS);
}

TEST(vec3, sub_direction) {
    /* 從 B 指向 A 的向量 = A - B */
    Vec3 a = {3, 0, 0};
    Vec3 b = {1, 0, 0};
    Vec3 dir = vec3_sub(a, b);
    ASSERT_VEC3_NEAR(dir, 2, 0, 0, EPS);
}

TEST(vec3, scale_positive) {
    Vec3 v = {1, 2, 3};
    Vec3 r = vec3_scale(v, 2.5);
    ASSERT_VEC3_NEAR(r, 2.5, 5.0, 7.5, EPS);
}

TEST(vec3, scale_zero) {
    Vec3 v = {1, 2, 3};
    Vec3 r = vec3_scale(v, 0.0);
    ASSERT_VEC3_NEAR(r, 0, 0, 0, EPS);
}

TEST(vec3, scale_negative) {
    Vec3 v = {1, 2, 3};
    Vec3 r = vec3_scale(v, -1.0);
    ASSERT_VEC3_NEAR(r, -1, -2, -3, EPS);
}

TEST(vec3, negate) {
    Vec3 v = {3, -1, 0};
    Vec3 r = vec3_negate(v);
    ASSERT_VEC3_NEAR(r, -3, 1, 0, EPS);
}

TEST(vec3, mul_hadamard) {
    /* 分量乘法（Hadamard product），用於顏色混合 */
    Vec3 a = {0.8, 0.5, 0.2};
    Vec3 b = {1.0, 2.0, 0.5};
    Vec3 r = vec3_mul(a, b);
    ASSERT_VEC3_NEAR(r, 0.8, 1.0, 0.1, EPS);
}

/* ══════════════════════════════════════
 *  點積（Dot Product）
 * ══════════════════════════════════════ */

TEST(vec3_dot, parallel_same_dir) {
    /* 同向單位向量：dot = 1 */
    Vec3 a = {1, 0, 0};
    Vec3 b = {1, 0, 0};
    ASSERT_NEAR(vec3_dot(a, b), 1.0, EPS);
}

TEST(vec3_dot, parallel_opposite) {
    /* 反向單位向量：dot = -1 */
    Vec3 a = { 1, 0, 0};
    Vec3 b = {-1, 0, 0};
    ASSERT_NEAR(vec3_dot(a, b), -1.0, EPS);
}

TEST(vec3_dot, orthogonal) {
    /* 正交向量：dot = 0 */
    Vec3 a = {1, 0, 0};
    Vec3 b = {0, 1, 0};
    ASSERT_NEAR(vec3_dot(a, b), 0.0, EPS);
}

TEST(vec3_dot, general) {
    Vec3 a = {1, 2, 3};
    Vec3 b = {4, 5, 6};
    /* 1*4 + 2*5 + 3*6 = 4 + 10 + 18 = 32 */
    ASSERT_NEAR(vec3_dot(a, b), 32.0, EPS);
}

TEST(vec3_dot, commutative) {
    Vec3 a = {1.5, -2.3, 4.7};
    Vec3 b = {3.1,  0.8, -1.2};
    ASSERT_NEAR(vec3_dot(a, b), vec3_dot(b, a), EPS);
}

/* ══════════════════════════════════════
 *  叉積（Cross Product）
 * ══════════════════════════════════════ */

TEST(vec3_cross, x_cross_y_equals_z) {
    /* 右手定則：X × Y = Z */
    Vec3 x = {1, 0, 0};
    Vec3 y = {0, 1, 0};
    Vec3 r = vec3_cross(x, y);
    ASSERT_VEC3_NEAR(r, 0, 0, 1, EPS);
}

TEST(vec3_cross, y_cross_x_equals_neg_z) {
    /* Y × X = -Z（反交換律） */
    Vec3 x = {1, 0, 0};
    Vec3 y = {0, 1, 0};
    Vec3 r = vec3_cross(y, x);
    ASSERT_VEC3_NEAR(r, 0, 0, -1, EPS);
}

TEST(vec3_cross, parallel_is_zero) {
    /* 平行向量叉積為零向量 */
    Vec3 a = {2, 0, 0};
    Vec3 b = {5, 0, 0};
    Vec3 r = vec3_cross(a, b);
    ASSERT_VEC3_NEAR(r, 0, 0, 0, EPS);
}

TEST(vec3_cross, result_orthogonal_to_inputs) {
    /* 叉積結果必須與兩個輸入都正交 */
    Vec3 a = {1, 2, 3};
    Vec3 b = {4, 5, 6};
    Vec3 c = vec3_cross(a, b);
    ASSERT_NEAR(vec3_dot(c, a), 0.0, EPS);
    ASSERT_NEAR(vec3_dot(c, b), 0.0, EPS);
}

TEST(vec3_cross, triangle_normal_direction) {
    /* 三角形法線方向測試：右手定則
     * 頂點逆時針排列 → 法線朝向觀察者 (+Z) */
    Vec3 v0 = {0, 0, 0};
    Vec3 v1 = {1, 0, 0};
    Vec3 v2 = {0, 1, 0};
    Vec3 e1 = vec3_sub(v1, v0);
    Vec3 e2 = vec3_sub(v2, v0);
    Vec3 n  = vec3_normalize(vec3_cross(e1, e2));
    /* 應朝向 +Z */
    ASSERT_NEAR(n.x, 0.0, EPS_F);
    ASSERT_NEAR(n.y, 0.0, EPS_F);
    ASSERT_NEAR(n.z, 1.0, EPS_F);
}

/* ══════════════════════════════════════
 *  長度與正規化
 * ══════════════════════════════════════ */

TEST(vec3_len, unit_x) {
    Vec3 v = {1, 0, 0};
    ASSERT_NEAR(vec3_len(v), 1.0, EPS);
}

TEST(vec3_len, general) {
    /* |(3,4,0)| = 5 */
    Vec3 v = {3, 4, 0};
    ASSERT_NEAR(vec3_len(v), 5.0, EPS_F);
}

TEST(vec3_len, zero_vector) {
    Vec3 v = {0, 0, 0};
    ASSERT_NEAR(vec3_len(v), 0.0, EPS);
}

TEST(vec3_normalize, result_is_unit_length) {
    Vec3 v = {3, 4, 0};
    Vec3 n = vec3_normalize(v);
    ASSERT_NEAR(vec3_len(n), 1.0, EPS_F);
}

TEST(vec3_normalize, direction_preserved) {
    Vec3 v = {3, 4, 0};
    Vec3 n = vec3_normalize(v);
    /* 方向保留：n.x/n.y 比例應與 3/4 相同 */
    ASSERT_NEAR(n.x / n.y, 3.0 / 4.0, EPS_F);
}

TEST(vec3_normalize, already_unit) {
    Vec3 v = {1, 0, 0};
    Vec3 n = vec3_normalize(v);
    ASSERT_VEC3_NEAR(n, 1, 0, 0, EPS_F);
}

TEST(vec3_normalize, zero_vector_safe) {
    /* 零向量正規化不應 crash，返回零向量 */
    Vec3 v = {0, 0, 0};
    Vec3 n = vec3_normalize(v);
    ASSERT_VEC3_NEAR(n, 0, 0, 0, EPS);
}

/* ══════════════════════════════════════
 *  反射向量
 * ══════════════════════════════════════ */

TEST(vec3_reflect, normal_incidence) {
    /* 垂直入射（沿 -Y 打到 Y 平面）→ 反射沿 +Y */
    Vec3 d = {0, -1, 0};   /* 入射方向（朝下） */
    Vec3 n = {0,  1, 0};   /* 法線（朝上） */
    Vec3 r = vec3_reflect(d, n);
    ASSERT_VEC3_NEAR(r, 0, 1, 0, EPS_F);
}

TEST(vec3_reflect, angle_preserved) {
    /* 反射定律：入射角 = 反射角
     * 入射方向 (1,-1,0)/√2，法線 (0,1,0)
     * 反射應為 (1,1,0)/√2                      */
    Vec3 d = vec3_normalize(vec3(1, -1, 0));
    Vec3 n = {0, 1, 0};
    Vec3 r = vec3_reflect(d, n);
    Vec3 expected = vec3_normalize(vec3(1, 1, 0));
    ASSERT_VEC3_NEAR(r, expected.x, expected.y, expected.z, EPS_F);
}

TEST(vec3_reflect, grazing_incidence) {
    /* 掠射（幾乎平行表面）：反射應與入射關於法線對稱 */
    Vec3 d = vec3_normalize(vec3(0.99, -0.1, 0));
    Vec3 n = {0, 1, 0};
    Vec3 r = vec3_reflect(d, n);
    /* 反射後 x 分量不變，y 分量取反 */
    ASSERT_NEAR(r.x,  d.x, EPS_F);
    ASSERT_NEAR(r.y, -d.y, EPS_F);
    ASSERT_NEAR(r.z,  d.z, EPS_F);
}

/* ══════════════════════════════════════
 *  Clamp
 * ══════════════════════════════════════ */

TEST(vec3_clamp, within_range) {
    Vec3 v = {0.5, 0.3, 0.9};
    Vec3 r = vec3_clamp(v, 0.0, 1.0);
    ASSERT_VEC3_NEAR(r, 0.5, 0.3, 0.9, EPS);
}

TEST(vec3_clamp, over_max) {
    Vec3 v = {1.5, 2.0, 0.8};
    Vec3 r = vec3_clamp(v, 0.0, 1.0);
    ASSERT_VEC3_NEAR(r, 1.0, 1.0, 0.8, EPS);
}

TEST(vec3_clamp, under_min) {
    Vec3 v = {-0.5, 0.5, -1.0};
    Vec3 r = vec3_clamp(v, 0.0, 1.0);
    ASSERT_VEC3_NEAR(r, 0.0, 0.5, 0.0, EPS);
}

int main(void) {
    RUN_SUITE(vec3,
        add_simple, add_negative,
        sub_simple, sub_direction,
        scale_positive, scale_zero, scale_negative,
        negate, mul_hadamard);
    RUN_SUITE(vec3_dot,
        parallel_same_dir, parallel_opposite,
        orthogonal, general, commutative);
    RUN_SUITE(vec3_cross,
        x_cross_y_equals_z, y_cross_x_equals_neg_z,
        parallel_is_zero, result_orthogonal_to_inputs,
        triangle_normal_direction);
    RUN_SUITE(vec3_len,
        unit_x, general, zero_vector);
    RUN_SUITE(vec3_normalize,
        result_is_unit_length, direction_preserved,
        already_unit, zero_vector_safe);
    RUN_SUITE(vec3_reflect,
        normal_incidence, angle_preserved, grazing_incidence);
    RUN_SUITE(vec3_clamp,
        within_range, over_max, under_min);
    return REPORT();
}
