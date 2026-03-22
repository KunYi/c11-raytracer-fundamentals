# 第 5 章：渲染管線、超採樣與 Gamma 校正

> **本章對應代碼：** `src/renderer.c` → `render()`、`write_ppm()`
> **核心問題：** 顏色值算出來後，如何正確地「印」到圖像上？為什麼不能直接把浮點數轉成整數？

---

## 5.1 渲染管線總覽

```
┌──────────────────────────────────────────────────────┐
│                  render() 主迴圈                      │
│                                                      │
│  for j in [height-1 .. 0]:   ← 從上到下掃描            │
│    for i in [0 .. width-1]:  ← 從左到右               │
│      ┌─────────────────────────────────┐             │
│      │  for s in [0 .. samples-1]:     │  超採樣      │
│      │    生成隨機偏移的光線              │             │
│      │    trace(ray) → 顏色            │              │
│      │    累加到 pixel                  │              │
│      └─────────────────────────────────┘              │
│      pixel /= samples   ← 平均                        │
│      儲存到 framebuffer                               │
│                                                      │
└──────────────────────────────────────────────────────┘
                    ↓
           write_ppm()
           ┌──────────────────────┐
           │  線性 RGB → Gamma 校正 │
           │  浮點 → 8-bit 整數    │
           │  寫出 PPM 二進位格式   │
           └──────────────────────┘
```

---

## 5.2 Framebuffer

Framebuffer 是一塊連續記憶體，儲存所有像素的顏色（線性 RGB，每分量一個 `double`）：

```c
// src/renderer.c
double *fb = malloc((size_t)width * height * 3 * sizeof(double));
//                                            ↑ R, G, B 各一個

// 像素 (i, j) 的 R 分量索引（j=0 是圖像頂部）
int idx = (j * width + i) * 3;
fb[idx + 0] = r;   // Red
fb[idx + 1] = g;   // Green
fb[idx + 2] = b;   // Blue
```

**座標系與記憶體的對應：**

渲染迴圈中 `j` 從 `height-1` 遞減到 `0`（螢幕座標：`j=height-1` 是頂部），
但記憶體中圖像通常從頂部行開始儲存，所以：

```c
int row = (height - 1 - j);    // 螢幕 j=height-1 → 記憶體 row=0（頂部）
int idx = (row * width + i) * 3;
```

這個翻轉讓螢幕座標和圖像記憶體座標一致。

---

## 5.3 超採樣抗鋸齒（Supersampling Anti-Aliasing）

### 5.3.1 為什麼有鋸齒？

圖像是離散的像素格，而幾何體是連續的。當一個三角形的邊界恰好穿過像素中心附近，那個像素「算不算被三角形覆蓋」只有是/否兩種答案。

結果：邊緣呈現明顯的階梯狀，即**鋸齒（Aliasing）**。

```
沒有 AA：              有 AA（超採樣）：
■■■□□□□□           ■■▓▒□□□□
■■■■□□□□           ■■■▓▒□□□
■■■■■□□□           ■■■■▓▒□□
```

### 5.3.2 Jitter 超採樣

**核心思路：** 對每個像素多次取樣，每次在像素內的隨機位置射出光線，最後取平均。

```
┌─────────────────┐
│  ·   ·  ·       │ 每個「·」代表一個取樣點
│    ·   ·        │ （在像素範圍內隨機分佈）
│  ·      ·  ·    │
│    ·  ·         │
└─────────────────┘
         ↓
    取所有取樣結果的平均
```

邊緣像素的不同取樣點，一部分落在三角形內（得到三角形顏色），一部分落在外（得到背景色），平均後得到中間色，形成平滑漸變。

### 5.3.3 數學基礎：取樣定理

根據 Nyquist–Shannon 取樣定理，要正確重建頻率為 $f$ 的信號，取樣頻率必須 $\geq 2f$。

圖像中，幾何邊緣的高頻細節在單像素採樣下被「混疊（aliased）」成低頻的鋸齒。超採樣等效於提高取樣頻率，減少混疊。

4× 超採樣讓有效取樣率提高 4 倍，可以正確重建更高頻的邊緣細節。

### 5.3.4 代碼實作

```c
// src/renderer.c — render()
for (int s = 0; s < samples; ++s) {
    // 在像素內隨機 jitter（Jitter Sampling）
    double jx = (samples > 1) ? rand01() : 0.5;
    double jy = (samples > 1) ? rand01() : 0.5;
    //          ↑ samples=1 時取中心點，不做隨機

    // 正規化座標：像素左下角 + 隨機偏移
    double u = ((double)i + jx) / (double)(width  - 1);
    double v = ((double)j + jy) / (double)(height - 1);

    Ray ray = camera_get_ray(cam, u, v);
    Vec3 c  = trace(scene, ray);
    pixel   = vec3_add(pixel, c);       // 累加
}
pixel = vec3_scale(pixel, 1.0 / samples);  // 取平均
```

### 5.3.5 偽隨機數產生器

我們用 **LCG（Linear Congruential Generator）**，一種極輕量的偽隨機數算法：

$$X_{n+1} = a \cdot X_n + c \pmod{2^{64}}$$

```c
static unsigned long long g_rand_state = 0;

static inline double rand01(void) {
    // 參數來自 Knuth 的經典 LCG
    g_rand_state = g_rand_state * 6364136223846793005ULL
                                + 1442695040888963407ULL;
    // 取高 31 位，轉換到 [0, 1)
    return (double)((g_rand_state >> 33) & 0x7FFFFFFF)
           / (double)0x7FFFFFFF;
}
```

**為什麼不用 `rand()`？**
- `rand()` 是全域狀態，多執行緒不安全
- 我們的 LCG 完全自包含，可以輕鬆並行化
- 不依賴 `<stdlib.h>` 的實作品質

---

## 5.4 Gamma 校正

### 5.4.1 問題：人眼的非線性感知

物理光照計算在**線性空間（Linear Space）**進行——數值 0.5 代表實際亮度的一半。

但人眼的亮度感知是**非線性**的：對暗處變化非常敏感，對亮處相對遲鈍。這是演化的結果（森林中區分陰影有生存價值）。

如果直接把線性亮度值 $L$ 對應到螢幕亮度 $L_{\text{screen}}$，暗部細節會丟失，亮部過於壓縮，圖像看起來整體偏暗。

### 5.4.2 sRGB 標準與 Gamma 值

現代螢幕和圖像格式使用 **sRGB** 色彩空間，其 gamma 值約為 2.2：

$$L_{\text{stored}} = L_{\text{linear}}^{1/\gamma} = L_{\text{linear}}^{1/2.2}$$

當螢幕顯示這個值時，它再做一次 gamma = 2.2 的冪次轉換，抵消我們的編碼：

$$L_{\text{display}} = L_{\text{stored}}^\gamma = (L_{\text{linear}}^{1/2.2})^{2.2} = L_{\text{linear}}$$

最終螢幕顯示的是物理正確的線性亮度。

### 5.4.3 為什麼是 2.2？

Gamma = 2.2 源於陰極射線管（CRT）螢幕的物理特性——輸入電壓 $V$ 與輸出亮度 $L$ 的關係恰好是 $L \propto V^{2.2}$。雖然 LCD 和 OLED 本身沒有這個特性，sRGB 標準仍然保留了這個約定，確保跨裝置一致性。

### 5.4.4 Gamma 校正的效果

| 線性值 $L$ | 未校正（直接 × 255） | Gamma 校正後（$L^{1/2.2}$ × 255） |
|-----------|--------------------|------------------------------------|
| 0.0 | 0 | 0 |
| 0.1 | 25 | 54 |
| 0.25 | 64 | 128 |
| 0.5 | 128 | 186 |
| 1.0 | 255 | 255 |

中間亮度被「拉亮」了——這補償了螢幕的 gamma = 2.2 壓暗，使最終顯示正確。

### 5.4.5 代碼實作

```c
// src/renderer.c — write_ppm()
for (int i = 0; i < width * height; ++i) {
    double r = fb[i * 3 + 0];
    double g = fb[i * 3 + 1];
    double b = fb[i * 3 + 2];

    // 1. Clamp 到 [0, 1]（防止過曝溢出）
    r = r < 0.0 ? 0.0 : (r > 1.0 ? 1.0 : r);
    g = g < 0.0 ? 0.0 : (g > 1.0 ? 1.0 : g);
    b = b < 0.0 ? 0.0 : (b > 1.0 ? 1.0 : b);

    // 2. Gamma 2.2 校正：linear → sRGB
    r = pow(r, 1.0 / 2.2);
    g = pow(g, 1.0 / 2.2);
    b = pow(b, 1.0 / 2.2);

    // 3. 量化到 8-bit [0, 255]
    unsigned char ir = (unsigned char)(r * 255.999);
    unsigned char ig = (unsigned char)(g * 255.999);
    unsigned char ib = (unsigned char)(b * 255.999);

    // 4. 寫入二進位 PPM
    fwrite(&ir, 1, 1, fp);
    fwrite(&ig, 1, 1, fp);
    fwrite(&ib, 1, 1, fp);
}
```

**為什麼是 `255.999` 而非 `256.0`？**

若值恰好為 `1.0`，乘以 `256.0` 得 `256`，截斷為整數後超出 `unsigned char` 範圍（0–255），溢出變成 `0`（純黑！）。

乘以 `255.999` 確保最大值 `1.0` 映射到 `255.999` → 截斷為 `255`，安全。

---

## 5.5 PPM 格式

PPM（Portable Pixmap）是最簡單的圖像格式，沒有壓縮，純二進位（P6 格式）：

```
P6\n
800 600\n         ← 寬 高
255\n             ← 最大值
[R G B R G B ...]  ← 每像素 3 bytes，從左到右、從上到下
```

```c
// src/renderer.c — write_ppm()
FILE *fp = fopen(path, "wb");       // "wb" = binary write
fprintf(fp, "P6\n%d %d\n255\n", width, height);
// 然後逐像素寫入 3 bytes
```

**優點：** 無需任何圖像庫，用任何文字編輯器可以讀標頭，任何圖像軟體可以開啟。
**缺點：** 無壓縮，一張 800×600 圖像 = 800 × 600 × 3 = 1,440,000 bytes（約 1.4 MB）。

目前直接加入 `write_png()` 取代 `write_ppm()` 直接產生壓縮的 `png` 不過前面的資料產生是一樣先按照 `ppm` 的。

---

## 5.6 效能分析

對於一張 800×600、8× 超採樣的圖像：

$$\text{光線總數} = 800 \times 600 \times 8 = 3,840,000 \text{ 條}$$

對每條光線：
- 對每個物件求交：$O(N)$，$N$ 為物件數
- 若擊中：對每個光源發射 Shadow Ray：$O(L \times N)$

總計算量約：

$$3,840,000 \times N \times (1 + L \times N)$$

我們的場景（$N=4$ 物件，$L=2$ 光源）：

$$3,840,000 \times 4 \times (1 + 2 \times 4) \approx 1.38 \times 10^8$$

約 1.38 億次向量運算，在現代 CPU 上大約 1–5 秒，符合實際渲染時間。

**若要加速：**
1. **BVH（Bounding Volume Hierarchy）**：將物件組織成樹狀結構，求交複雜度從 $O(N)$ 降為 $O(\log N)$
2. **多執行緒**：每行（或每塊像素）分配一個執行緒，CPU 使用率 $N_{\text{core}}$ 倍
3. **GPU**：數千個 CUDA core 同時渲染不同像素，速度可提升 100–1000 倍

---

## 5.7 本章總結

```
render() 主迴圈
   ↓
per-pixel：jitter 超採樣 samples 次
   ↓ 平均
線性 RGB 顏色值（double，[0, 1]）
   ↓ clamp
   ↓ pow(x, 1/2.2)    ← Gamma 校正（線性空間 → sRGB）
   ↓ × 255.999         ← 量化（浮點 → 8-bit）
   ↓ 寫入 PPM 二進位格式
圖像檔案
```

**三個「容易犯錯的細節」：**

| 錯誤 | 症狀 | 修正 |
|------|------|------|
| 忘記 Gamma 校正 | 圖像整體偏暗，暗部細節消失 | `pow(x, 1/2.2)` |
| 用 256.0 量化 | 純白像素變純黑 | 改用 `255.999` |
| 忘記偏移 Shadow Ray 起點 | 表面出現大量黑色噪點 | 沿法線偏移 `1e-4` |

---

## 終章：完整資料流

```
main.c
  場景定義（幾何、材質、光源、相機參數）
        ↓
camera_init()
  位置 + lookat + FOV → lower_left, horizontal, vertical
        ↓
render()  [對每個像素 × samples 次]
  camera_get_ray(s, t)
        ↓
  trace(scene, ray)
    scene_hit() → 找最近交點 HitRecord
        ↓
    phong_shade()
      Ambient + Diffuse + Specular
      Shadow Ray 判斷遮擋
        ↓
    vec3_clamp() → [0, 1]
        ↓
  超採樣平均
        ↓
write_ppm()
  Gamma 2.2 校正
  8-bit 量化
  二進位 PPM 寫出
        ↓
  output.ppm → 你的圖像瀏覽器
```

---

> **恭喜你讀完整個系列！**
>
> 你現在理解了一個完整的 Ray Tracer 從向量運算到最終圖像的每一個步驟。
>
> **延伸閱讀方向：**
> - [Ray Tracing in One Weekend](https://raytracing.github.io/)（英文，免費，進階實作）
> - [Physically Based Rendering: From Theory to Implementation](https://pbr-book.org/)（PBR 聖經，免費線上版）
> - 嘗試為這個 Ray Tracer 加入：反射材質、景深（Defocus Blur）、BVH 加速結構
