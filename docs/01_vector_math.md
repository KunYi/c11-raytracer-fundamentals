# 第 1 章：向量數學——Ray Tracer 的語言

> **本章對應代碼：** `include/vec3.h`
> **核心問題：** 3D 空間中的位置、方向、距離，該怎麼用數學描述？

---

## 1.1 為什麼需要向量？

Ray Tracing 的一切都發生在三維空間：光線有方向、物體有位置、法線有朝向。
我們需要一套能夠**精確描述空間關係**的數學工具——這就是**向量（Vector）**。

在我們的程式裡，`Vec3` 身兼三種角色：

| 角色 | 範例 | 意義 |
|------|------|------|
| **位置點** | 相機位於 `(0, 1, 5)` | 空間中的座標 |
| **方向向量** | 光線朝 `(0, -1, 0)` | 有方向但無起點 |
| **顏色值** | `(0.8, 0.4, 0.1)` | R, G, B 分量 |

同一個 struct，三種物理意義，這種「多型」是 Ray Tracer 代碼簡潔的關鍵。

---

## 1.2 Vec3 的定義

```c
// include/vec3.h
typedef struct {
    double x, y, z;
} Vec3;
```

用 `double`（64 位元浮點數）而非 `float`（32 位元）的原因：
- 求交運算涉及大量相減，精度損失會導致「自交偽影（self-intersection artifact）」
- 現代 CPU 對 double 的運算速度幾乎與 float 相同

---

## 1.3 基本運算

### 加法與減法

$$\vec{a} + \vec{b} = (a_x + b_x,\ a_y + b_y,\ a_z + b_z)$$

$$\vec{a} - \vec{b} = (a_x - b_x,\ a_y - b_y,\ a_z - b_z)$$

**物理意義：**
- 加法：位移疊加（從 A 走向量 $\vec{b}$ 抵達 B）
- 減法：求位移（從 B 到 A 的向量 = $A - B$）

```c
static inline Vec3 vec3_add(Vec3 a, Vec3 b) {
    return (Vec3){a.x + b.x, a.y + b.y, a.z + b.z};
}

static inline Vec3 vec3_sub(Vec3 a, Vec3 b) {
    return (Vec3){a.x - b.x, a.y - b.y, a.z - b.z};
}
```

**實際應用：** 計算光線從交點到光源的方向向量：

```c
Vec3 L_vec = vec3_sub(light->position, rec->point);
//           光源位置  -    交點位置   =  指向光源的向量
```

---

### 純量縮放

$$s \cdot \vec{v} = (s \cdot v_x,\ s \cdot v_y,\ s \cdot v_z)$$

```c
static inline Vec3 vec3_scale(Vec3 v, double s) {
    return (Vec3){v.x * s, v.y * s, v.z * s};
}
```

**應用：** 沿光線方向走 $t$ 單位到達交點：

```c
// ray_at(ray, t) = origin + t * direction
Vec3 point = vec3_add(ray.origin, vec3_scale(ray.dir, t));
```

這一行就是整個 Ray Tracer 最核心的座標計算。

---

### 分量乘法（Hadamard Product）

$$\vec{a} \odot \vec{b} = (a_x b_x,\ a_y b_y,\ a_z b_z)$$

```c
static inline Vec3 vec3_mul(Vec3 a, Vec3 b) {
    return (Vec3){a.x * b.x, a.y * b.y, a.z * b.z};
}
```

這**不是**向量數學的標準乘法，而是**顏色混合**專用的運算：

```c
// 材質漫反射顏色 × 光源顏色 = 實際照明顏色
Vec3 diffuse_color = vec3_mul(mat->diffuse, light->color);
// (0.8, 0.4, 0.1) × (1.4, 1.3, 1.2) = (1.12, 0.52, 0.12)
// 紅色物體在暖白光下 → 更深的橙紅
```

---

## 1.4 點積（Dot Product）——最重要的運算

$$\vec{a} \cdot \vec{b} = a_x b_x + a_y b_y + a_z b_z$$

等價的幾何定義：

$$\vec{a} \cdot \vec{b} = |\vec{a}| \cdot |\vec{b}| \cdot \cos\theta$$

其中 $\theta$ 是兩向量的夾角。

```c
static inline double vec3_dot(Vec3 a, Vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
```

### 點積的三個關鍵用途

**① 判斷方向關係（正交性檢測）**

| $\vec{a} \cdot \vec{b}$ | 意義 |
|------------------------|------|
| $> 0$ | 同側（夾角 $< 90°$） |
| $= 0$ | 正交（夾角 $= 90°$） |
| $< 0$ | 反側（夾角 $> 90°$） |

應用：判斷光線是否從法線正面打來：

```c
// 若光線方向與法線同向（dot > 0），說明光線從背面打入
// 需要翻轉法線使其始終面向光線來源
if (vec3_dot(ray.dir, out_normal) > 0.0)
    out_normal = vec3_negate(out_normal);
```

**② 計算漫射光強度（Lambert's Law）**

$$I_{\text{diffuse}} = \max(0,\ \hat{N} \cdot \hat{L})$$

$\hat{N}$ 是單位法線，$\hat{L}$ 是指向光源的單位向量。

物理直覺：光線越「正對」表面（$\theta$ 越小，$\cos\theta$ 越大），照射面積越小，單位面積能量越高，越亮。

```c
double NdotL = vec3_dot(N, L);
if (NdotL < 0.0) NdotL = 0.0;   // 背光面設為 0，不接受負光
Vec3 diffuse = vec3_scale(
    vec3_mul(mat->diffuse, light->color),
    att * NdotL                           // 乘以 cos(θ)
);
```

**③ 向量長度的快速計算**

$$|\vec{v}|^2 = \vec{v} \cdot \vec{v} = v_x^2 + v_y^2 + v_z^2$$

避免開平方根（昂貴運算）的場合下，比較 $|\vec{v}|^2$ 與常數即可。

---

## 1.5 叉積（Cross Product）——法線的來源

$$\vec{a} \times \vec{b} = \begin{pmatrix} a_y b_z - a_z b_y \\ a_z b_x - a_x b_z \\ a_x b_y - a_y b_x \end{pmatrix}$$

**幾何意義：**
- 結果向量**垂直於** $\vec{a}$ 和 $\vec{b}$ 所在平面
- 方向由**右手定則**決定：四指從 $\vec{a}$ 彎向 $\vec{b}$，拇指指向結果
- 長度 $= |\vec{a}||\vec{b}|\sin\theta$（等於兩向量圍成的平行四邊形面積）

```c
static inline Vec3 vec3_cross(Vec3 a, Vec3 b) {
    return (Vec3){
        a.y * b.z - a.z * b.y,   // x 分量
        a.z * b.x - a.x * b.z,   // y 分量
        a.x * b.y - a.y * b.x    // z 分量
    };
}
```

### 計算三角形法線

```c
// include/scene.h — triangle_init()
Vec3 edge1 = vec3_sub(v1, v0);   // 邊 v0→v1
Vec3 edge2 = vec3_sub(v2, v0);   // 邊 v0→v2

// 叉積結果：垂直於三角形平面的向量
// 右手定則決定法線朝哪側（頂點順序決定「正面」）
Vec3 normal = vec3_normalize(vec3_cross(edge1, edge2));
```

```
        v2
       /  \
      /    \
    v0 ——— v1

edge1 = v1 - v0  →
edge2 = v2 - v0  ↗

cross(edge1, edge2) 指向紙面外（朝向讀者）
```

### 建立相機座標系

叉積還用來建立相機的正交基底（右手座標系）：

```c
// include/camera.h — camera_init()
Vec3 w = vec3_normalize(vec3_sub(position, lookat));  // 相機「後方」
Vec3 u = vec3_normalize(vec3_cross(up, w));           // 相機「右方」
Vec3 v = vec3_cross(w, u);                            // 相機「上方」
```

為什麼 `u` 需要 normalize 但 `v` 不需要？因為 `w` 和 `u` 都已是單位向量，且互相正交，所以 `cross(w, u)` 的長度恰好為 $|\vec{w}||\vec{u}|\sin 90° = 1$，天然就是單位向量。

---

## 1.6 向量長度與正規化

$$|\vec{v}| = \sqrt{v_x^2 + v_y^2 + v_z^2} = \sqrt{\vec{v} \cdot \vec{v}}$$

```c
static inline double vec3_len(Vec3 v) {
    return sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
}
```

**正規化（Normalize）**——將向量縮放成單位長度（長度 = 1）：

$$\hat{v} = \frac{\vec{v}}{|\vec{v}|}$$

```c
static inline Vec3 vec3_normalize(Vec3 v) {
    double len = vec3_len(v);
    if (len < 1e-12) return (Vec3){0, 0, 0};  // 零向量保護
    return vec3_scale(v, 1.0 / len);
}
```

> **為什麼要有 `1e-12` 的保護？**
> 如果不小心對零向量（或極小向量）正規化，會除以零，產生 `NaN` 或 `Inf`，污染整張圖像。

**什麼時候需要正規化？**

| 情況 | 需要正規化 | 原因 |
|------|-----------|------|
| 光線方向 `ray.dir` | 是 | 點積公式假設單位向量 |
| 表面法線 `normal` | 是 | Lambert 定律假設單位向量 |
| 光線到光源向量 `L` | 是 | 同上 |
| 顏色值 | 否 | 不是方向，無需單位長度 |
| 距離計算中途值 | 視情況 | 若只比較大小，可跳過 |

---

## 1.7 反射向量——高光計算的基礎

當光線 $\vec{d}$（指向光源反方向，即入射方向）碰到法線為 $\hat{N}$ 的表面，反射方向為：

$$\vec{r} = \vec{d} - 2(\vec{d} \cdot \hat{N})\hat{N}$$

**推導：**

設入射向量 $\vec{d}$ 分解為平行法線分量和垂直法線分量：

$$\vec{d} = \underbrace{(\vec{d} \cdot \hat{N})\hat{N}}_{\vec{d}_\parallel} + \underbrace{\vec{d} - (\vec{d} \cdot \hat{N})\hat{N}}_{\vec{d}_\perp}$$

反射時，平行分量取反，垂直分量不變：

$$\vec{r} = -\vec{d}_\parallel + \vec{d}_\perp = \vec{d} - 2(\vec{d} \cdot \hat{N})\hat{N}$$

```c
static inline Vec3 vec3_reflect(Vec3 d, Vec3 n) {
    // d: 入射方向（已 normalize）
    // n: 表面法線（已 normalize）
    return vec3_sub(d, vec3_scale(n, 2.0 * vec3_dot(d, n)));
}
```

在光照計算中使用：

```c
// renderer.c — phong_shade()
// L = 從交點指向光源的單位向量
// 我們需要「從表面反射出去，朝向入射光來向」的向量 R
Vec3 R = vec3_reflect(vec3_negate(L), N);
// vec3_negate(L) = 入射方向（與 L 相反）
// 結果 R = 反射方向

double RdotV = vec3_dot(R, view_dir);  // 反射方向與視線方向的夾角
// RdotV 越大（越對齊），高光越亮
```

---

## 1.8 `static inline` 的設計選擇

`vec3.h` 裡所有函式都是 `static inline`：

```c
static inline Vec3 vec3_add(Vec3 a, Vec3 b) { ... }
```

- **`inline`**：提示編譯器在呼叫處直接展開代碼，避免函式呼叫的 stack 開銷。對每像素執行數百次的向量運算，這個優化至關重要。
- **`static`**：每個編譯單元（`.c` 檔）都有自己的副本，避免多個 `.c` 引入同一 `.h` 時產生符號重複定義的連結錯誤。

配合 `-O3 -ffast-math` 編譯選項，現代 GCC/Clang 會進一步將這些運算向量化（SIMD），同時計算多個像素。

---

## 1.9 本章總結

| 運算 | 公式 | 主要用途 |
|------|------|---------|
| 加法 `+` | 分量相加 | 位移、顏色疊加 |
| 減法 `-` | 分量相減 | 計算方向向量 |
| 縮放 `×s` | 分量乘純量 | 沿方向移動 |
| 分量乘 `⊙` | 分量對應相乘 | 顏色混合 |
| 點積 `·` | $\sum a_i b_i = \cos\theta$ | 角度、光強、求交判斷 |
| 叉積 `×` | 行列式展開 | 法線計算、相機座標系 |
| 長度 $\|\cdot\|$ | $\sqrt{\vec{v}\cdot\vec{v}}$ | 距離、正規化前置 |
| 正規化 $\hat{v}$ | $\vec{v}/\|\vec{v}\|$ | 方向向量標準化 |
| 反射 | $\vec{d}-2(\vec{d}\cdot\hat{N})\hat{N}$ | Phong 高光計算 |

> **下一章：** [02 — 光線模型與 Pinhole Camera](./02_ray_and_camera.md)
> 我們將用這些向量工具，構造「從眼睛射出的問詢光線」。
