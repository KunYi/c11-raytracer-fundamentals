# 第 4 章：光照物理與 Phong 模型

> **本章對應代碼：** `src/renderer.c` → `phong_shade()`，`include/scene.h` → `Material`
> **核心問題：** 交點確定後，它應該是什麼顏色？光如何與物體表面作用？

---

## 4.1 光照的物理本質

真實的光照極為複雜——光子在材質內部散射、折射、被吸收，再以各種角度射出。完整模擬這個過程（路徑追蹤、光子映射）計算量極大。

**Phong 光照模型**（1975 年由 Bui Tuong Phong 提出）是一種**經驗模型**：不從物理第一原理出發，而是用簡單的數學公式近似人眼感知到的光照效果。它把光照分解為三個獨立的分量：

$$I = I_{\text{ambient}} + I_{\text{diffuse}} + I_{\text{specular}}$$

---

## 4.2 環境光（Ambient Light）

### 物理動機

真實世界中，即使背光處也不是純黑——光線在場景中多次反彈後，均勻地照亮所有角落。正確模擬這個效果需要全域光照（Global Illumination），計算成本很高。

環境光用一個**常數**近似這個效果：

$$I_{\text{ambient}} = I_a \cdot K_a$$

| 符號 | 意義 | 程式中 |
|------|------|--------|
| $I_a$ | 環境光強度（全域常數） | `scene->ambient_light` |
| $K_a$ | 材質的環境光係數 | `mat->ambient` |

```c
// src/renderer.c — phong_shade()
Vec3 color = vec3_mul(scene->ambient_light, mat->ambient);
// (0.10, 0.10, 0.13) × (0.08, 0.05, 0.01) = (0.008, 0.005, 0.001)
// 極暗的底色，防止完全黑暗
```

**調整技巧：**
- `ambient` 太高 → 陰影處亮得不自然（「無陰影感」）
- `ambient` 太低 → 陰影太黑（「洞窟感」）
- 通常設在 `0.05–0.15` 範圍

---

## 4.3 漫射光（Diffuse Light）——Lambert's Law

### 物理動機

粗糙表面（木頭、石頭、布料）會將光**均勻散射**到各個方向。從任何角度看，亮度相同——這種表面叫做**Lambertian Surface**（蘭伯特面）。

### Lambert's Cosine Law

照射到面上的光強度與入射角的餘弦成正比：

$$I_{\text{diffuse}} = I_l \cdot K_d \cdot \max(0, \hat{N} \cdot \hat{L})$$

| 符號 | 意義 |
|------|------|
| $I_l$ | 光源強度 |
| $K_d$ | 材質漫反射係數（顏色） |
| $\hat{N}$ | 表面單位法線 |
| $\hat{L}$ | 指向光源的單位向量 |
| $\hat{N} \cdot \hat{L} = \cos\theta$ | 入射角的餘弦 |

**幾何直覺：**

```
正對光源（θ=0°）：      斜照（θ=60°）：
  光線 ↓↓↓↓↓↓↓          光線 ↘↘↘↘↘
  ─────────────          ─────────────
  面積 A，接收全部光      相同光但分散在更大面積
  亮度 = I × 1.0         亮度 = I × cos(60°) = 0.5
```

相同數量的光子，斜照時分佈在更大的面積上，每單位面積能量更少，所以更暗。

```c
// src/renderer.c — phong_shade()
Vec3   Lv  = vec3_sub(light->position, rec->point); // 指向光源的向量
double dist = vec3_len(Lv);
Vec3   L    = vec3_scale(Lv, 1.0 / dist);           // 正規化

double NdotL = vec3_dot(N, L);
if (NdotL < 0.0) NdotL = 0.0;   // 背光面 cos(θ) < 0，clamped 到 0

Vec3 diffuse = vec3_scale(
    vec3_mul(mat->diffuse, light->color),  // K_d × I_l（顏色混合）
    att * NdotL                            // × 衰減 × cos(θ)
);
```

---

## 4.4 高光（Specular Light）——鏡面反射

### 物理動機

光滑表面（金屬、亮漆、水面）的反射有方向性——只有在特定角度才能看到光源的「亮點」（Highlight）。

### Phong 高光公式

$$I_{\text{specular}} = I_l \cdot K_s \cdot \max(0, \hat{R} \cdot \hat{V})^n$$

| 符號 | 意義 |
|------|------|
| $K_s$ | 材質鏡面係數 |
| $\hat{R}$ | 光線的反射方向（由法線和入射方向決定） |
| $\hat{V}$ | 指向觀察者的方向（= `-ray.dir`） |
| $n$ | 高光指數（Shininess），控制高光大小 |

**$\hat{R}$ 的計算：**

$$\hat{R} = \hat{L}' - 2(\hat{L}' \cdot \hat{N})\hat{N}$$

其中 $\hat{L}' = -\hat{L}$（入射方向，與 $\hat{L}$ 相反）。

```c
// src/renderer.c
Vec3   R     = vec3_reflect(vec3_negate(L), N);   // 反射向量
double RdotV = vec3_dot(R, view_dir);             // 與視線夾角
if (RdotV < 0.0) RdotV = 0.0;

Vec3 specular = vec3_scale(
    vec3_mul(mat->specular, light->color),
    att * pow(RdotV, mat->shininess)   // (R·V)^n
);
```

**`shininess` 指數的效果：**

| shininess | 高光面積 | 表面感覺 |
|-----------|---------|---------|
| 4–8 | 很大，模糊 | 粗糙塑料 |
| 16–32 | 中等 | 皮革 |
| 64–128 | 小，清晰 | 拋光金屬 |
| 256–512 | 非常小 | 鏡面、玻璃 |

**數學原因：** $(\cos\theta)^n$ 隨 $n$ 增大，在 $\theta$ 接近 $0$ 時才有顯著值，高光點因此收縮。

---

## 4.5 光源衰減（Attenuation）

真實世界中，光強度隨距離增加而衰減。根據物理定律，**平方反比衰減（Inverse Square Law）**：

$$I \propto \frac{1}{d^2}$$

但純平方反比在近距離 $d \to 0$ 時會產生無窮大，所以遊戲與 CG 工業常用的是**修正公式**：

$$\text{att}(d) = \frac{1}{1 + k_l \cdot d + k_q \cdot d^2}$$

| 係數 | 效果 |
|------|------|
| $k_l = 0$ 且 $k_q = 0$ | 無衰減（平行光） |
| $k_l > 0$，$k_q = 0$ | 線性衰減，柔和 |
| $k_l = 0$，$k_q > 0$ | 物理正確的平方反比 |
| $k_l > 0$，$k_q > 0$ | 混合，可調近/遠衰減速度 |

```c
// src/renderer.c
double kl  = 0.09;
double kq  = 0.032;
double att = 1.0 / (1.0 + kl * dist + kq * dist * dist);
```

**我們的設定**（$k_l=0.09$，$k_q=0.032$）是 OpenGL 教學中常見的「中等範圍」參數，在距離 $d=5$ 處大約剩餘 50% 亮度。

---

## 4.6 Shadow Ray（硬陰影）

### 原理

在計算光照之前，先確認交點到光源之間**是否被其他物體擋住**：

從交點 $P$ 向光源 $L$ 射出一條**陰影光線（Shadow Ray）**，若它碰到任何物體（且碰撞點在光源之前），則該光源對 $P$ 無貢獻。

```c
// src/renderer.c — phong_shade()
Vec3 shadow_origin = vec3_add(rec->point, vec3_scale(N, 1e-4));
//                                         ↑ 沿法線偏移，避免自交
Ray  shadow_ray    = ray_make(shadow_origin, L);

HitRecord shadow_rec;
if (scene_hit(scene, shadow_ray, 1e-4, dist - 1e-4, &shadow_rec))
    continue;   // 被遮擋 → 跳過此光源的 diffuse 和 specular
//                  t_max = dist - 1e-4 確保不「穿過」光源到另一側
```

**注意 `t_max = dist - 1e-4`：** 若設為無限大，光線可能打到光源「後方」的物體，誤判為陰影。

### 硬陰影 vs 軟陰影

我們實作的是**硬陰影（Hard Shadow）**：點光源，要麼全亮要麼全暗，邊界清晰。

真實陰影是**軟陰影（Soft Shadow）**：光源有面積，陰影邊界有半影（Penumbra）——既有「完全被遮擋」的本影（Umbra），也有「部分被遮擋」的漸變區。

實作軟陰影：對面積光源多點取樣，對每個採樣點發射一條陰影光線，最後取平均。

---

## 4.7 完整 Phong 公式

將三個分量和衰減合在一起：

$$\boxed{
I = I_a K_a + \sum_{\text{lights}} \text{att}(d) \left[ I_l K_d \max(0, \hat{N}\cdot\hat{L}) + I_l K_s \max(0, \hat{R}\cdot\hat{V})^n \right]
}$$

```c
// src/renderer.c — phong_shade() 完整流程
Vec3 color = vec3_mul(scene->ambient_light, mat->ambient);  // 環境光

for (int li = 0; li < scene->light_count; ++li) {
    // ... 計算 L, dist, att
    // ... Shadow Ray 測試，若被遮擋則 continue

    // Diffuse
    double NdotL = fmax(0.0, vec3_dot(N, L));
    Vec3 diffuse = vec3_scale(vec3_mul(mat->diffuse, light->color), att * NdotL);

    // Specular
    Vec3 R = vec3_reflect(vec3_negate(L), N);
    double RdotV = fmax(0.0, vec3_dot(R, view_dir));
    Vec3 specular = vec3_scale(vec3_mul(mat->specular, light->color),
                               att * pow(RdotV, mat->shininess));

    color = vec3_add(color, vec3_add(diffuse, specular));
}
return color;
```

---

## 4.8 材質設計實例

```c
// 金橙色圓錐側面：高漫射 + 高光
Material cone_side = {
    .ambient   = {0.07, 0.03, 0.01},   // 暗橙底色
    .diffuse   = {0.90, 0.35, 0.08},   // 飽和橙紅漫射
    .specular  = {1.00, 0.80, 0.40},   // 帶金色調的高光
    .shininess = 120.0                  // 小而清晰的高光點
};

// 玻璃感藍色球體
Material sphere_mat = {
    .ambient   = {0.02, 0.05, 0.10},   // 極暗藍底
    .diffuse   = {0.10, 0.35, 0.75},   // 深藍漫射（降低漫射比例）
    .specular  = {0.95, 0.95, 1.00},   // 幾乎白色的高光（趨近白光反射）
    .shininess = 256.0                  // 極小高光點 → 玻璃/金屬感
};
```

**為什麼球的 `diffuse` 偏低、`specular` 偏高？**
玻璃和金屬的物理特性：入射光大部分被反射而非散射。降低漫反射、提高鏡面反射，近似這種材質的外觀。

---

## 4.9 Phong 模型的局限

| 問題 | 原因 | 更好的解法 |
|------|------|---------|
| 無全域光照 | 環境光是常數近似 | 路徑追蹤（Path Tracing） |
| 無間接照明 | 物體不互相照亮 | 光子映射、輻射度 |
| 高光不物理 | $(R\cdot V)^n$ 非 BRDF | Cook-Torrance 模型 |
| 無散射介質 | 無霧、無次表面散射 | 體積渲染 |
| 硬陰影 | 點光源 | 面積光源 + 多重採樣 |

儘管如此，Phong 模型在 1975 年到 2000 年代初期是遊戲和 CG 的主流。理解它是理解後續 PBR（Physically Based Rendering）材質系統的基礎。

---

## 4.10 本章總結

```
交點 P，法線 N，視線方向 V
        ↓
環境光：I_a × K_a（常數底色）
        +
對每個光源：
  ├── Shadow Ray → 被遮擋？→ 跳過
  ├── 計算衰減 att(d)
  ├── 漫射：att × K_d × I_l × max(0, N·L)
  └── 高光：att × K_s × I_l × max(0, R·V)^n
        ↓
最終顏色 = 三者之和，clamp 到 [0,1]
```

> **下一章：** [05 — 渲染管線、超採樣與 Gamma 校正](./05_rendering.md)
> 顏色計算完成後，如何正確地輸出到螢幕？這涉及取樣理論和色彩科學。
