#ifndef RENDERER_H
#define RENDERER_H

#include "vec3.h"
#include "ray.h"
#include "scene.h"
#include "camera.h"

/* 場景：改用統一 Object 陣列 */
typedef struct {
    const Object    *objects;
    int              obj_count;
    const PointLight *lights;
    int              light_count;
    Vec3             ambient_light;
    Vec3             bg_color;
} Scene;

/*
 * render — 渲染場景到 framebuffer
 *   framebuffer：呼叫者分配，大小 width*height*3 個 double
 *   每像素 3 個 double（線性 RGB，範圍 [0,1]）
 */
void render(const Camera *cam, const Scene *scene,
            int width, int height, int samples,
            double *framebuffer);

int write_ppm(const char *path, const double *framebuffer,
              int width, int height);

/*
 * write_png — 將 framebuffer 輸出為 PNG
 *   內部執行：Gamma 2.2 校正 → 8-bit 量化 → zlib 壓縮 → PNG 編碼
 *   返回 0 成功，-1 失敗
 */
int write_png(const char *path, const double *framebuffer,
              int width, int height);
#endif
