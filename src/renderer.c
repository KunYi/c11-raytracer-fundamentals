#include "renderer.h"
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <zlib.h>

/* 場景求最近交點 */
static int scene_hit(const Scene *scene, Ray ray,
                     double t_min, double t_max, HitRecord *rec)
{
    HitRecord tmp;
    int hit=0; double closest=t_max;
    for(int i=0;i<scene->obj_count;++i){
        if(object_intersect(&scene->objects[i],ray,t_min,closest,&tmp)){
            hit=1; closest=tmp.t; *rec=tmp;
        }
    }
    return hit;
}

/* Phong 光照 + 硬陰影 */
static Vec3 phong_shade(const Scene *scene, const HitRecord *rec, Vec3 view_dir)
{
    const Material *mat=&rec->mat;
    Vec3 N=rec->normal;
    Vec3 color=vec3_mul(scene->ambient_light,mat->ambient);

    for(int li=0;li<scene->light_count;++li){
        const PointLight *light=&scene->lights[li];
        Vec3 Lv=vec3_sub(light->position,rec->point);
        double dist=vec3_len(Lv);
        Vec3 L=vec3_scale(Lv,1.0/dist);

        Vec3 shadow_origin=vec3_add(rec->point,vec3_scale(N,1e-4));
        Ray  shadow_ray=ray_make(shadow_origin,L);
        HitRecord sr;
        if(scene_hit(scene,shadow_ray,1e-4,dist-1e-4,&sr)) continue;

        double kl=0.09,kq=0.032;
        double att=1.0/(1.0+kl*dist+kq*dist*dist);

        double NdotL=vec3_dot(N,L); if(NdotL<0.0)NdotL=0.0;
        Vec3 diffuse=vec3_scale(vec3_mul(mat->diffuse,light->color),att*NdotL);

        Vec3 R=vec3_reflect(vec3_negate(L),N);
        double RdotV=vec3_dot(R,view_dir); if(RdotV<0.0)RdotV=0.0;
        Vec3 specular=vec3_scale(vec3_mul(mat->specular,light->color),
                                  att*pow(RdotV,mat->shininess));

        color=vec3_add(color,vec3_add(diffuse,specular));
    }
    return color;
}

static Vec3 trace(const Scene *scene, Ray ray){
    HitRecord rec;
    if(scene_hit(scene,ray,1e-4,1e14,&rec)){
        Vec3 c=phong_shade(scene,&rec,vec3_negate(ray.dir));
        return vec3_clamp(c,0.0,1.0);
    }
    return scene->bg_color;
}

/* ══════════════════════════════════════════════════
 *  偽隨機數（LCG）
 * ══════════════════════════════════════════════════ */
static unsigned long long g_rand=0;
static inline double rand01(void){
    g_rand=g_rand*6364136223846793005ULL+1442695040888963407ULL;
    /* 取高 31 位，除以 2^31（而非 2^31-1），保證值域嚴格為 [0, 1)。
     * 若分母用 0x7FFFFFFF，最大狀態值會產生恰好 1.0，
     * 使邊緣像素 u/v > 1，破壞 [0,1) 的取樣保證。              */
    return (double)((g_rand>>33)&0x7FFFFFFF) / 2147483648.0;
}

void render(const Camera *cam, const Scene *scene,
            int width, int height, int samples, double *fb)
{
    /* ── 參數防衛性檢查 ──────────────────────────────────────────
     * width <= 1 或 height <= 1：(width-1) 或 (height-1) 為 0，
     *   u/v 計算會除以零，產生 NaN/Inf 污染整張圖。
     * samples <= 0：pixel /= samples 除以零，行為未定義。
     * fb == NULL：後續寫入會 segfault。
     * ──────────────────────────────────────────────────────────── */
    if (!fb || width < 1 || height < 1 || samples < 1) {
        fprintf(stderr, "render: 無效參數 "
                "(width=%d, height=%d, samples=%d, fb=%p)\n",
                width, height, samples, (void*)fb);
        return;
    }

    /* width=1 或 height=1 時，除數為 0；改用 max(dim-1, 1) 使單像素
     * 情況對應 u=v=0.5（螢幕中心），行為合理且無除以零。           */
    double inv_w = (width  > 1) ? 1.0 / (double)(width  - 1) : 1.0;
    double inv_h = (height > 1) ? 1.0 / (double)(height - 1) : 1.0;

    g_rand=(unsigned long long)time(NULL);
    for(int j=height-1;j>=0;--j){
        for(int i=0;i<width;++i){
            Vec3 pixel={0,0,0};
            for(int s=0;s<samples;++s){
                double jx=(samples>1)?rand01():0.5;
                double jy=(samples>1)?rand01():0.5;
                double u=((double)i+jx)*inv_w;
                double v=((double)j+jy)*inv_h;
                Ray ray=camera_get_ray(cam,u,v);
                pixel=vec3_add(pixel,trace(scene,ray));
            }
            pixel=vec3_scale(pixel,1.0/samples);
            int row=(height-1-j);
            int idx=(row*width+i)*3;
            fb[idx+0]=pixel.x; fb[idx+1]=pixel.y; fb[idx+2]=pixel.z;
        }
        if((height-1-j)%(height/10+1)==0){
            fprintf(stderr,"\r  進度：%3d%%",(int)(100.0*(height-j)/height));
            fflush(stderr);
        }
    }
    fprintf(stderr,"\r  進度：100%%\n");
}

int write_ppm(const char *path, const double *fb, int width, int height){
    FILE *fp=fopen(path,"wb");
    if(!fp){perror(path);return -1;}
    fprintf(fp,"P6\n%d %d\n255\n",width,height);
    for(int i=0;i<width*height;++i){
        double r=fb[i*3+0],g=fb[i*3+1],b=fb[i*3+2];
        r=pow(r<0?0:(r>1?1:r),1.0/2.2);
        g=pow(g<0?0:(g>1?1:g),1.0/2.2);
        b=pow(b<0?0:(b>1?1:b),1.0/2.2);
        unsigned char ir=(unsigned char)(r*255.999);
        unsigned char ig=(unsigned char)(g*255.999);
        unsigned char ib=(unsigned char)(b*255.999);
        fwrite(&ir,1,1,fp); fwrite(&ig,1,1,fp); fwrite(&ib,1,1,fp);
    }
    fclose(fp);
    return 0;
}

/* ══════════════════════════════════════════════════
 *  PNG 輸出
 *
 *  PNG 規格（RFC 2083）：
 *    簽名(8) + IHDR chunk + IDAT chunk(s) + IEND chunk
 *    每個 chunk = 長度(4 BE) + 類型(4) + 資料(N) + CRC32(4 BE)
 *    IDAT 資料 = zlib deflate 壓縮的「過濾行」
 *    每行過濾行 = 濾波器類型(1 byte) + 像素 RGB(width*3 bytes)
 * ══════════════════════════════════════════════════ */

/* ── CRC-32（IEEE 802.3 多項式 0xEDB88320）── */
static uint32_t s_crc_table[256];
static int      s_crc_ready = 0;

static void crc_init(void){
    for(uint32_t n=0;n<256;++n){
        uint32_t c=n;
        for(int k=0;k<8;++k)
            c=(c&1)?(0xEDB88320u^(c>>1)):(c>>1);
        s_crc_table[n]=c;
    }
    s_crc_ready=1;
}

static uint32_t crc32_buf(uint32_t crc, const uint8_t *buf, size_t len){
    if(!s_crc_ready) crc_init();
    crc^=0xFFFFFFFFu;
    for(size_t i=0;i<len;++i)
        crc=s_crc_table[(crc^buf[i])&0xFF]^(crc>>8);
    return crc^0xFFFFFFFFu;
}

/* ── Big-Endian uint32 寫出 ── */
static void write_u32be(FILE *fp, uint32_t v){
    uint8_t b[4]={(uint8_t)(v>>24),(uint8_t)(v>>16),
                  (uint8_t)(v>>8), (uint8_t)(v)};
    fwrite(b,1,4,fp);
}

/* ── PNG chunk 寫出 ── */
static void png_chunk(FILE *fp, const char *type,
                      const uint8_t *data, uint32_t len){
    write_u32be(fp, len);
    fwrite(type, 1, 4, fp);
    if(len) fwrite(data, 1, len, fp);
    uint32_t crc = crc32_buf(0,(const uint8_t*)type,4);
    if(len)  crc = crc32_buf(crc, data, len);
    write_u32be(fp, crc);
}

/* ══════════════════════════════════════════════════
 *  write_png
 *
 *  framebuffer：線性 RGB double，[0,1] 範圍
 *  在此函式內做：Gamma 2.2 校正 → 量化 → PNG 編碼 → 輸出
 * ══════════════════════════════════════════════════ */
int write_png(const char *path, const double *fb, int width, int height)
{
    FILE *fp = fopen(path, "wb");
    if(!fp){ perror(path); return -1; }

    /* ── 步驟 1：Gamma 校正 + 量化 → 8-bit sRGB 像素陣列 ── */
    size_t npix = (size_t)width * height * 3;
    uint8_t *pixels = malloc(npix);
    if(!pixels){ fclose(fp); return -1; }

    for(int i=0; i<width*height; ++i){
        double r = fb[i*3+0], g = fb[i*3+1], b = fb[i*3+2];
        /* clamp → gamma 2.2 → 8-bit */
        r = pow(r<0?0:(r>1?1:r), 1.0/2.2);
        g = pow(g<0?0:(g>1?1:g), 1.0/2.2);
        b = pow(b<0?0:(b>1?1:b), 1.0/2.2);
        pixels[i*3+0] = (uint8_t)(r*255.999);
        pixels[i*3+1] = (uint8_t)(g*255.999);
        pixels[i*3+2] = (uint8_t)(b*255.999);
    }

    /* ── 步驟 2：插入 PNG 濾波器前綴（Filter 0 = None）──
     *
     *  PNG 每行像素前加 1 byte 的濾波器類型標記。
     *  Filter 0（無濾波）最簡單：直接複製原始像素。
     *  更進階的濾波器（Sub/Up/Average/Paeth）可提升壓縮率，
     *  但對渲染圖像 Filter 0 已足夠。
     */
    size_t row_stride = (size_t)width * 3;           /* 一行的像素 bytes */
    size_t filtered_size = (size_t)height * (1 + row_stride); /* 每行多 1 byte */
    uint8_t *filtered = malloc(filtered_size);
    if(!filtered){ free(pixels); fclose(fp); return -1; }

    for(int row=0; row<height; ++row){
        filtered[row*(1+row_stride)] = 0;             /* 濾波器類型 = 0 */
        memcpy(filtered + row*(1+row_stride) + 1,
               pixels  + row*row_stride,
               row_stride);
    }
    free(pixels);

    /* ── 步驟 3：zlib DEFLATE 壓縮 ──
     *
     *  PNG IDAT 資料就是 zlib 格式（RFC 1950）：
     *    2 bytes zlib 標頭 + DEFLATE 壓縮資料 + 4 bytes Adler-32
     *  compress2() 直接輸出這個格式。
     */
    uLongf comp_bound = compressBound((uLongf)filtered_size);
    uint8_t *compressed = malloc(comp_bound);
    if(!compressed){ free(filtered); fclose(fp); return -1; }

    uLongf comp_size = comp_bound;
    int zret = compress2(compressed, &comp_size,
                         filtered, (uLongf)filtered_size,
                         Z_BEST_COMPRESSION);
    free(filtered);

    if(zret != Z_OK){
        fprintf(stderr, "zlib 壓縮失敗（錯誤碼 %d）\n", zret);
        free(compressed); fclose(fp); return -1;
    }

    /* ── 步驟 4：寫出 PNG 結構 ── */

    /* PNG 簽名（固定 8 bytes，所有 PNG 檔案開頭） */
    static const uint8_t sig[8] = {0x89,'P','N','G',0x0D,0x0A,0x1A,0x0A};
    fwrite(sig, 1, 8, fp);

    /* IHDR chunk（圖像基本資訊，固定 13 bytes）
     *   寬(4) 高(4) 位元深度(1)=8 色彩類型(1)=2(RGB)
     *   壓縮方法(1)=0 濾波方法(1)=0 交錯(1)=0
     */
    uint8_t ihdr[13];
    ihdr[0]=(uint8_t)(width>>24); ihdr[1]=(uint8_t)(width>>16);
    ihdr[2]=(uint8_t)(width>>8);  ihdr[3]=(uint8_t)(width);
    ihdr[4]=(uint8_t)(height>>24);ihdr[5]=(uint8_t)(height>>16);
    ihdr[6]=(uint8_t)(height>>8); ihdr[7]=(uint8_t)(height);
    ihdr[8]=8; ihdr[9]=2; ihdr[10]=0; ihdr[11]=0; ihdr[12]=0;
    png_chunk(fp, "IHDR", ihdr, 13);

    /* IDAT chunk（壓縮後的像素資料） */
    png_chunk(fp, "IDAT", compressed, (uint32_t)comp_size);
    free(compressed);

    /* IEND chunk（結束標記，資料長度為 0） */
    png_chunk(fp, "IEND", NULL, 0);

    fclose(fp);
    return 0;
}
