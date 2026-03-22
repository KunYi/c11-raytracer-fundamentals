/*
 * ============================================================
 *  Ray Tracer — 圓錐體 + 球形  多角度測試
 *
 *  場景：
 *    左側  圓錐體  (Cone)   — 頂點 (−1.5, 2.5, 0)，底面 y=0，半徑 1.0
 *    右側  球形    (Sphere) — 圓心 ( 1.8, 0.9, 0)，半徑 0.9
 *    地板  兩個三角形（無限大平面近似）
 *
 *  6 個相機角度批次輸出：view_0.ppm … view_5.ppm
 * ============================================================ */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "vec3.h"
#include "ray.h"
#include "camera.h"
#include "scene.h"
#include "renderer.h"

int main(void)
{
    const int W=800, H=600, SPP=8;

    /* ── 材質 ── */

    /* 圓錐側面：暖橙紅，高金屬感高光 */
    Material cone_side = {
        {0.07,0.03,0.01}, {0.90,0.35,0.08}, {1.00,0.80,0.40}, 120.0
    };
    /* 圓錐底面：暗棕 */
    Material cone_base = {
        {0.04,0.02,0.01}, {0.30,0.12,0.04}, {0.08,0.05,0.02},  12.0
    };

    /* 球形：冷青藍，玻璃質感高光 */
    Material sphere_mat = {
        {0.02,0.05,0.10}, {0.10,0.35,0.75}, {0.95,0.95,1.00}, 256.0
    };

    /* 地板：淺米灰 */
    Material floor_mat = {
        {0.10,0.10,0.09}, {0.75,0.72,0.68}, {0.15,0.15,0.15},  10.0
    };

    /* ── 場景物件 ── */
#define NOBJ 5
    Object objects[NOBJ];
    int n=0;

    /* 圓錐：頂點在上，底面在 y=0 */
    objects[n++] = object_cone(
        vec3(-1.5, 2.5, 0.0),  /* apex  */
        0.0,                    /* base_y */
        1.0,                    /* radius */
        cone_side, cone_base
    );

    /* 球形 */
    objects[n++] = object_sphere(vec3(1.8, 0.9, 0.0), 0.9, sphere_mat);

    /* 地板（兩個巨形三角形） */
    objects[n++] = object_triangle(
        vec3(-8,0,-8), vec3(8,0,-8), vec3(8,0,8), floor_mat);
    objects[n++] = object_triangle(
        vec3(-8,0,-8), vec3(8,0,8), vec3(-8,0,8), floor_mat);

    /* ── 點光源 ── */
    PointLight lights[2] = {
        { {4.0, 7.0, 5.0}, {1.4, 1.3, 1.1} },   /* 主光：右上前暖白 */
        { {-3.0, 3.5,-1.0}, {0.3, 0.4, 0.6} },   /* 補光：左後冷藍（弱） */
    };

    /* ── 場景 ── */
    Scene scene = {
        .objects      = objects,
        .obj_count    = n,
        .lights       = lights,
        .light_count  = 2,
        .ambient_light = {0.08,0.09,0.12},
        .bg_color      = {0.03,0.04,0.09}
    };

    /* ── Framebuffer ── */
    double *fb=(double*)malloc((size_t)W*H*3*sizeof(double));
    if(!fb){fprintf(stderr,"OOM\n");return 1;}

    /* ── 6 個相機視角 ── */
    typedef struct { double px,py,pz; double ax,ay,az; double fov;
                     const char *file; const char *desc; } Shot;

    Shot shots[]={
        /* 正前方 45° 標準展示 */
        { 0.0, 3.0, 8.0,   0.0,1.2,0.0,  40, "view_0.png", "正前 45° 標準展示"    },
        /* 右側低角，看球的高光 + 錐體側面 */
        { 6.0, 1.5, 4.0,   0.0,1.2,0.0,  50, "view_1.png", "右側低角（球體高光）" },
        /* 左後高角，看錐體背面 + 地板陰影 */
        {-5.0, 5.0,-3.5,   0.0,1.0,0.0,  48, "view_2.png", "左後俯角（錐體背）"   },
        /* 正上俯視，看錐頂與球頂 */
        { 0.3, 9.5, 0.01,  0.0,0.5,0.0,  44, "view_3.png", "正上俯視（頂視）"     },
        /* 極低草地角，看輪廓剪影 */
        { 0.0, 0.3, 8.5,   0.0,1.0,0.0,  36, "view_4.png", "極低草地角（剪影）"   },
        /* 右前近距廣角，強透視感 */
        { 2.5, 2.5, 3.8,   0.0,1.2,0.0,  70, "view_5.png", "近距廣角（透視感）"   },
    };
    int nshots=(int)(sizeof(shots)/sizeof(shots[0]));
    double aspect=(double)W/(double)H;

    fprintf(stderr,
        "================================================\n"
        "  Ray Tracer — 圓錐體 + 球形\n"
        "  解析度 %dx%d  超採樣 %dx  物件數 %d\n"
        "================================================\n\n",
        W,H,SPP,n);

    for(int i=0;i<nshots;++i){
        Shot *s=&shots[i];
        fprintf(stderr,"[%d/%d] %s\n       %s\n",i+1,nshots,s->desc,s->file);
        Camera cam=camera_init(
            vec3(s->px,s->py,s->pz),
            vec3(s->ax,s->ay,s->az),
            vec3(0,1,0), s->fov, aspect);
        render(&cam,&scene,W,H,SPP,fb);
        // if(write_ppm(s->file,fb,W,H)==0)
        //     fprintf(stderr,"       已輸出 %s\n\n",s->file);
        if(write_png(s->file,fb,W,H)==0)
             fprintf(stderr,"       已輸出 %s\n\n",s->file);
    }

    fprintf(stderr,"全部完成！\n");
    free(fb);
    return 0;
}
