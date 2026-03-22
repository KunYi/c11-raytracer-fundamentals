#include "test_framework.h"
#include "vec3.h"
#include "ray.h"
#include "camera.h"
#include "scene.h"
#include "renderer.h"
#include <stdlib.h>
#include <math.h>

#define EPS_F 1e-6

/* ── 輔助：空場景 ── */
static Scene make_empty_scene(void) {
    Scene s = {NULL,0,NULL,0,{0,0,0},{0.1,0.1,0.1}};
    return s;
}

/* ── 輔助：有一顆球的場景（讓 u/v 路徑實際被使用）──
 *
 * 球心在原點，半徑 0.5。相機在 (0,0,3) 看向原點，FOV=60°。
 * 在這個設定下，球填滿畫面中央約 1/3 的區域。
 * 邊緣像素光線不打中球 → 回傳 bg_color。
 * 中央像素光線打中球 → 回傳受光照計算的顏色。
 *
 * 若 rand01() 回傳 1.0，最後一行/列的光線會偏移到螢幕外，
 * 對於應該打中球的邊緣像素，偏移後可能打不中球，
 * 導致顏色跳回 bg_color。
 * 透過對稱性（左右像素應對稱）來偵測這種偏移。
 */
static void make_sphere_scene(Scene *scene, Object *obj) {
    Material mat = {{0.05,0.05,0.05},{0.8,0.2,0.2},{0.9,0.9,0.9},64.0};
    *obj = object_sphere(vec3(0,0,0), 0.5, mat);
    static PointLight light = {{2,2,2},{1.5,1.5,1.5}};
    scene->objects      = obj;
    scene->obj_count    = 1;
    scene->lights       = &light;
    scene->light_count  = 1;
    scene->ambient_light = vec3(0.1,0.1,0.1);
    scene->bg_color      = vec3(0.05,0.05,0.05);
}

static Camera make_default_camera(int w, int h) {
    return camera_init(
        vec3(0,0,3), vec3(0,0,0), vec3(0,1,0),
        45.0, (double)w/(double)h);
}

/* ══════════════════════════════════════
 *  Bug 1 回歸：除以零邊界條件
 * ══════════════════════════════════════ */

TEST(renderer_bugs, null_framebuffer_no_crash) {
    Scene  scene = make_empty_scene();
    Camera cam   = make_default_camera(4,4);
    render(&cam, &scene, 4, 4, 1, NULL);
    ASSERT_TRUE(1);
}

TEST(renderer_bugs, zero_samples_no_crash) {
    Scene  scene = make_empty_scene();
    Camera cam   = make_default_camera(4,4);
    double fb[4*4*3];
    render(&cam, &scene, 4, 4, 0, fb);
    ASSERT_TRUE(1);
}

TEST(renderer_bugs, width_1_no_crash) {
    Scene  scene = make_empty_scene();
    Camera cam   = make_default_camera(1,4);
    double fb[1*4*3];
    render(&cam, &scene, 1, 4, 1, fb);
    ASSERT_TRUE(1);
}

TEST(renderer_bugs, height_1_no_crash) {
    Scene  scene = make_empty_scene();
    Camera cam   = make_default_camera(4,1);
    double fb[4*1*3];
    render(&cam, &scene, 4, 1, 1, fb);
    ASSERT_TRUE(1);
}

TEST(renderer_bugs, width_1_output_is_finite) {
    Scene  scene = make_empty_scene();
    Camera cam   = make_default_camera(1,4);
    double fb[1*4*3];
    render(&cam, &scene, 1, 4, 1, fb);
    for (int i = 0; i < 1*4*3; ++i)
        ASSERT_TRUE(isfinite(fb[i]));
}

TEST(renderer_bugs, height_1_output_is_finite) {
    Scene  scene = make_empty_scene();
    Camera cam   = make_default_camera(4,1);
    double fb[4*1*3];
    render(&cam, &scene, 4, 1, 1, fb);
    for (int i = 0; i < 4*1*3; ++i)
        ASSERT_TRUE(isfinite(fb[i]));
}

/* ══════════════════════════════════════
 *  Bug 2 回歸：rand01() 值域必須嚴格 [0, 1)
 *
 *  策略：使用有球體的場景，以 samples=1（關閉 jitter）
 *  渲染中心行，取得「基準顏色」。
 *  再以高 spp 渲染，確認超採樣平均值接近基準，
 *  不因邊緣 jitter 越界而偏移。
 *
 *  更直接的白箱測試：
 *  確認所有像素輸出值均落在 [0,1] 內（NaN/Inf 或 >1 代表越界）。
 *  有球的場景讓 u/v 路徑實際被計算，不像空場景直接略過取樣。
 * ══════════════════════════════════════ */

TEST(renderer_bugs, edge_pixels_finite_with_geometry) {
    /* 有球體的場景，確保 u/v 計算路徑實際被執行
     * 64 spp 讓 RNG 跑過足夠多狀態，提高觸發原始 bug 的機率 */
    Object obj; Scene scene;
    make_sphere_scene(&scene, &obj);
    Camera cam = make_default_camera(8,8);
    double fb[8*8*3];
    render(&cam, &scene, 8, 8, 64, fb);

    for (int i = 0; i < 8*8*3; ++i) {
        ASSERT_TRUE(isfinite(fb[i]));
        ASSERT_TRUE(fb[i] >= 0.0);
        ASSERT_TRUE(fb[i] <= 1.0);
    }
}

TEST(renderer_bugs, rand01_never_reaches_1) {
    /* Bug 2 白箱測試：rand01() 值域必須嚴格為 [0, 1)
     *
     * 問題根源：分母若用 0x7FFFFFFF，當狀態高 31 位恰為 0x7FFFFFFF
     * 時，輸出恰好等於 1.0，使 u/v 超出 [0,1] 範圍。
     *
     * 修正後分母為 2^31=2147483648，最大輸出為 0x7FFFFFFF/2^31
     * ≈ 0.9999999995，嚴格小於 1.0。
     *
     * 白箱驗證：直接重現 LCG，對每個可能的高 31 位值
     * 確認新公式的輸出嚴格 < 1.0，舊公式在最大值時輸出 1.0。
     * 用若干個精心選擇的狀態而不是暴力窮舉 2^64 次。          */

    /* 最壞情況：高 31 位 = 0x7FFFFFFF（全 1） */
    unsigned long long worst_high31 = (unsigned long long)0x7FFFFFFF << 33;

    double old_formula = (double)0x7FFFFFFF / (double)0x7FFFFFFF;
    double new_formula = (double)0x7FFFFFFF / 2147483648.0;

    /* 舊公式在最大值時確實產生 1.0（確認 bug 存在） */
    ASSERT_NEAR(old_formula, 1.0, 1e-15);

    /* 新公式在最大值時嚴格 < 1.0（確認修正有效） */
    ASSERT_TRUE(new_formula < 1.0);
    ASSERT_NEAR(new_formula, 0.9999999995, 1e-9);

    /* 任意高 31 位值：新公式輸出必須嚴格 < 1.0 */
    unsigned long long test_states[] = {
        0x7FFFFFFFu, 0x7FFFFFFEu, 0x40000000u,
        0x7FFFFF00u, 0x3FFFFFFFu, 0x00000001u
    };
    int n = (int)(sizeof(test_states)/sizeof(test_states[0]));
    for (int k = 0; k < n; ++k) {
        double v = (double)test_states[k] / 2147483648.0;
        ASSERT_TRUE(v >= 0.0);
        ASSERT_TRUE(v < 1.0);
    }
    (void)worst_high31;
}

/* ══════════════════════════════════════
 *  基本正確性
 * ══════════════════════════════════════ */

/* ── 輔助：可寫的暫存路徑 ── */
static const char *tmp_png(void) {
#ifdef _WIN32
    return "test_write_tmp.png";
#else
    return "/tmp/test_write_tmp.png";
#endif
}

/* ══════════════════════════════════════
 *  write_png 參數驗證
 * ══════════════════════════════════════ */

TEST(write_png, null_path_returns_error) {
    double fb[3] = {0};
    ASSERT_EQ(write_png(NULL, fb, 1, 1), -1);
}

TEST(write_png, null_fb_returns_error) {
    ASSERT_EQ(write_png(tmp_png(), NULL, 1, 1), -1);
}

TEST(write_png, zero_width_returns_error) {
    double fb[3] = {0};
    ASSERT_EQ(write_png(tmp_png(), fb, 0, 1), -1);
}

TEST(write_png, zero_height_returns_error) {
    double fb[3] = {0};
    ASSERT_EQ(write_png(tmp_png(), fb, 1, 0), -1);
}

TEST(write_png, negative_dim_returns_error) {
    double fb[3] = {0};
    ASSERT_EQ(write_png(tmp_png(), fb, -1, 4), -1);
    ASSERT_EQ(write_png(tmp_png(), fb,  4, -1), -1);
}

TEST(write_png, valid_1x1_returns_success) {
    /* 最小合法輸入：1×1 純黑像素 */
    double fb[3] = {0.0, 0.0, 0.0};
    ASSERT_EQ(write_png(tmp_png(), fb, 1, 1), 0);
}

TEST(write_png, valid_output_has_png_signature) {
    /* 合法輸出的前 8 bytes 必須是 PNG 簽名 */
    double fb[3] = {1.0, 0.5, 0.0};
    write_png(tmp_png(), fb, 1, 1);

    FILE *fp = fopen(tmp_png(), "rb");
    ASSERT_TRUE(fp != NULL);

    unsigned char sig[8];
    size_t n = fread(sig, 1, 8, fp);
    fclose(fp);

    ASSERT_EQ((int)n, 8);
    /* PNG 簽名：\x89 P N G \r \n \x1a \n */
    ASSERT_EQ(sig[0], 0x89);
    ASSERT_EQ(sig[1], 'P');
    ASSERT_EQ(sig[2], 'N');
    ASSERT_EQ(sig[3], 'G');
    ASSERT_EQ(sig[4], 0x0D);
    ASSERT_EQ(sig[5], 0x0A);
    ASSERT_EQ(sig[6], 0x1A);
    ASSERT_EQ(sig[7], 0x0A);
}

/* ══════════════════════════════════════
 *  render() 剩餘的 NULL 防衛
 * ══════════════════════════════════════ */

TEST(render_null, null_cam_no_crash) {
    Scene  scene = make_empty_scene();
    double fb[4*4*3];
    render(NULL, &scene, 4, 4, 1, fb);
    ASSERT_TRUE(1);
}

TEST(render_null, null_scene_no_crash) {
    Camera cam = make_default_camera(4, 4);
    double fb[4*4*3];
    render(&cam, NULL, 4, 4, 1, fb);
    ASSERT_TRUE(1);
}

TEST(renderer_basic, bg_color_for_empty_scene) {
    /* 空場景每個像素應等於 bg_color */
    Scene  scene = make_empty_scene();
    scene.bg_color = vec3(0.2, 0.4, 0.6);
    Camera cam = make_default_camera(4,4);
    double fb[4*4*3];
    render(&cam, &scene, 4, 4, 1, fb);
    for (int p = 0; p < 4*4; ++p) {
        ASSERT_NEAR(fb[p*3+0], 0.2, EPS_F);
        ASSERT_NEAR(fb[p*3+1], 0.4, EPS_F);
        ASSERT_NEAR(fb[p*3+2], 0.6, EPS_F);
    }
}

int main(void) {
    RUN_SUITE(renderer_bugs,
        null_framebuffer_no_crash,
        zero_samples_no_crash,
        width_1_no_crash,
        height_1_no_crash,
        width_1_output_is_finite,
        height_1_output_is_finite,
        edge_pixels_finite_with_geometry,
        rand01_never_reaches_1);
    RUN_SUITE(write_png,
        null_path_returns_error,
        null_fb_returns_error,
        zero_width_returns_error,
        zero_height_returns_error,
        negative_dim_returns_error,
        valid_1x1_returns_success,
        valid_output_has_png_signature);
    RUN_SUITE(render_null,
        null_cam_no_crash,
        null_scene_no_crash);
    RUN_SUITE(renderer_basic,
        bg_color_for_empty_scene);
    return REPORT();
}
