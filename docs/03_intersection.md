# 第 3 章：幾何求交——三角形、球、圓錐

> **本章對應代碼：** `include/scene.h`
> **核心問題：** 光線 $\vec{P}(t) = \vec{O} + t\vec{D}$ 與幾何體相交，$t$ 值是多少？

求交（Intersection）是 Ray Tracer 最核心的計算。每發射一條光線，都要對場景中所有物體求交，找到最近的那個，才能知道「這條光線碰到了什麼」。

---

## 3.1 求交的統一框架

所有幾何體的求交函式都遵循同一個合約（Contract）：

```c
int geometry_intersect(
    const Geometry *geom,   // 幾何體
    Ray ray,                // 光線
    double t_min,           // 有效 t 的下界（避免自交）
    double t_max,           // 有效 t 的上界（只找最近的）
    HitRecord *rec          // 輸出：交點資訊
);
// 返回 1 = 擊中，rec 已填寫
// 返回 0 = 未擊中
```

`HitRecord` 記錄交點的一切資訊：

```c
typedef struct {
    double   t;       // 光線參數（交點距起點的距離）
    Vec3     point;   // 交點世界座標 = ray_at(ray, t)
    Vec3     normal;  // 交點法線（單位向量，朝向光線來源側）
    Material mat;     // 交點材質
} HitRecord;
```

---

## 3.2 三角形求交：從直覺到高效演算法

### 3.2.0 第一版：先求平面交點，再測試是否在三角形內

理解 Möller–Trumbore 之前，先看最自然的思路——這也是本專案**最初只有三角形時**所依賴的數學基礎。

**直覺分解為兩步：**

```
步驟 1：光線與三角形所在的無限平面求交  → 得到交點 P
步驟 2：判斷 P 是否在三角形邊界以內    → 最終答案
```

---

#### 步驟 1：光線與平面求交

三角形所在的平面可以用**法線 $\hat{N}$ 和平面上一點 $V_0$**來描述。
平面方程式（點法式）：

$$\hat{N} \cdot (\vec{P} - V_0) = 0$$

展開後等價於常見的 $ax + by + cz = d$ 形式，其中 $(a,b,c) = \hat{N}$，$d = \hat{N} \cdot V_0$。

將光線 $\vec{P}(t) = \vec{O} + t\vec{D}$ 代入：

$$\hat{N} \cdot (\vec{O} + t\vec{D} - V_0) = 0$$

$$\hat{N} \cdot (\vec{O} - V_0) + t\,(\hat{N} \cdot \vec{D}) = 0$$

解出 $t$：

$$\boxed{t = \frac{\hat{N} \cdot (V_0 - \vec{O})}{\hat{N} \cdot \vec{D}}}$$

**特殊情況：** 若 $\hat{N} \cdot \vec{D} \approx 0$，光線與平面平行（分母為零），無交點。

```c
// 最初版本的平面求交（概念代碼）
Vec3   N    = tri->normal;               // 預計算的單位法線
double denom = vec3_dot(N, ray.dir);     // N · D

if (fabs(denom) < EPSILON) return 0;     // 光線平行平面

Vec3   v0_to_O = vec3_sub(tri->v0, ray.origin);
double t       = vec3_dot(N, v0_to_O) / denom;  // 平面公式

if (t < t_min || t > t_max) return 0;

Vec3 P = ray_at(ray, t);                 // 平面上的交點
```

**法線從哪來？** 就是第 1 章學的叉積——在 `triangle_init()` 預先計算並存好：

$$\hat{N} = \text{normalize}((V_1 - V_0) \times (V_2 - V_0))$$

```c
// include/scene.h — triangle_init()
tri->normal = vec3_normalize(
    vec3_cross(
        vec3_sub(v1, v0),   // E1 = V1 - V0
        vec3_sub(v2, v0)    // E2 = V2 - V0
    )
);
```

---

#### 步驟 2：點在三角形內的測試

得到平面交點 $P$ 後，要判斷它是否在三角形 $V_0V_1V_2$ 的內部。

**方法：邊測試（Edge Test / Cross Product Test）**

對三角形的每條邊，做「$P$ 是否在邊的左側」的測試：

$$\hat{N} \cdot \left[(V_1 - V_0) \times (P - V_0)\right] \geq 0 \quad \text{（邊 }V_0 V_1\text{）}$$

$$\hat{N} \cdot \left[(V_2 - V_1) \times (P - V_1)\right] \geq 0 \quad \text{（邊 }V_1 V_2\text{）}$$

$$\hat{N} \cdot \left[(V_0 - V_2) \times (P - V_2)\right] \geq 0 \quad \text{（邊 }V_2 V_0\text{）}$$

三條邊都通過 → $P$ 在三角形內。

**幾何直覺：**

```
        V2
       /  \
      /  P ← 在內部：叉積方向與 N 同向（dot > 0）
     / ×   \
   V0 ——————V1

若 P 在邊 V0V1 的右側（外部），
(V1-V0) × (P-V0) 的方向與 N 反向，dot < 0 → 判定為外部
```

```c
// 邊測試（概念代碼）
Vec3 N = tri->normal;

// 邊 V0→V1
Vec3 edge0 = vec3_sub(tri->v1, tri->v0);
Vec3 vp0   = vec3_sub(P, tri->v0);
if (vec3_dot(N, vec3_cross(edge0, vp0)) < 0) return 0;  // P 在邊外

// 邊 V1→V2
Vec3 edge1 = vec3_sub(tri->v2, tri->v1);
Vec3 vp1   = vec3_sub(P, tri->v1);
if (vec3_dot(N, vec3_cross(edge1, vp1)) < 0) return 0;

// 邊 V2→V0
Vec3 edge2 = vec3_sub(tri->v0, tri->v2);
Vec3 vp2   = vec3_sub(P, tri->v2);
if (vec3_dot(N, vec3_cross(edge2, vp2)) < 0) return 0;

// 通過三條邊測試 → P 在三角形內
rec->t      = t;
rec->point  = P;
rec->normal = (vec3_dot(ray.dir, N) > 0.0) ? vec3_negate(N) : N;
rec->mat    = tri->mat;
return 1;
```

#### 第一版的完整運算量

| 步驟 | 運算 | 次數 |
|------|------|------|
| 法線點積（分母） | 1 次點積 | 1 |
| $t$ 計算 | 1 次點積 + 1 次除法 | 2 |
| 三條邊各一次叉積 + 點積 | 3 × (叉積 + 點積) | 6 |
| **合計** | | **9 次向量運算** |

這個方法正確，但有**兩個缺點**：

1. **需要預先計算並儲存法線**（`tri->normal` 欄位佔記憶體）
2. **運算量比 Möller–Trumbore 多**（後者只需要 7 次向量運算，且不需要除法）

---

#### 從第一版到 Möller–Trumbore：關鍵洞察

第一版把問題分成兩步。Möller–Trumbore 的天才在於：**把「在哪個平面上」和「是否在三角形內」這兩個問題合併成一個線性方程組同時解出**，避免了重複計算，且中間結果（$u, v$）天然就是重心座標，可以做插值（UV 貼圖、法線插值等）。

---

### 3.2.1 問題描述

給定三角形頂點 $V_0, V_1, V_2$，以及光線 $\vec{P}(t) = \vec{O} + t\vec{D}$，
求參數 $t$（以及重心座標 $u, v$）使得光線落在三角形內。

### 3.2.2 重心座標系

三角形內任意一點可以用**重心座標**（Barycentric Coordinates）表示：

$$\vec{P} = (1-u-v)\,V_0 + u\,V_1 + v\,V_2$$

其中 $u \geq 0$，$v \geq 0$，$u + v \leq 1$。

三個係數 $(1-u-v, u, v)$ 分別是點 $P$ 相對於三個頂點的「權重」，三者之和 $= 1$。

### 3.2.3 建立方程組

將光線代入，交點滿足：

$$\vec{O} + t\vec{D} = (1-u-v)\,V_0 + u\,V_1 + v\,V_2$$

移項整理，令 $\vec{E_1} = V_1 - V_0$，$\vec{E_2} = V_2 - V_0$，$\vec{S} = \vec{O} - V_0$：

$$t\vec{D} - u\vec{E_1} - v\vec{E_2} = \vec{S}$$

這是一個對 $(t, u, v)$ 的**線性方程組**，可以用 Cramer's Rule 求解：

$$\begin{pmatrix} -\vec{D} & \vec{E_1} & \vec{E_2} \end{pmatrix} \begin{pmatrix} t \\ u \\ v \end{pmatrix} = \vec{S}$$

### 3.2.4 Cramer's Rule 求解

對 $3 \times 3$ 線性系統 $M\vec{x} = \vec{b}$，Cramer's Rule 給出：

$$x_i = \frac{\det(M_i)}{\det(M)}$$

其中 $M_i$ 是將 $M$ 的第 $i$ 列替換為 $\vec{b}$ 所得的矩陣。

結合混合積（Scalar Triple Product）性質 $\vec{a} \cdot (\vec{b} \times \vec{c}) = \vec{b} \cdot (\vec{c} \times \vec{a})$，可以化簡為：

$$a = \det(-\vec{D}, \vec{E_1}, \vec{E_2}) = \vec{E_1} \cdot (\vec{D} \times \vec{E_2}) = \vec{E_1} \cdot \vec{h}$$

其中 $\vec{h} = \vec{D} \times \vec{E_2}$。

最終：

$$\boxed{
a = \vec{E_1} \cdot \vec{h}, \quad
u = \frac{\vec{S} \cdot \vec{h}}{a}, \quad
\vec{q} = \vec{S} \times \vec{E_1}, \quad
v = \frac{\vec{D} \cdot \vec{q}}{a}, \quad
t = \frac{\vec{E_2} \cdot \vec{q}}{a}
}$$

### 3.2.5 代碼對照

```c
// include/scene.h — triangle_intersect()
static inline int triangle_intersect(const Triangle *tri, Ray ray,
                                     double t_min, double t_max,
                                     HitRecord *rec)
{
    Vec3 e1 = vec3_sub(tri->v1, tri->v0);   // E1 = V1 - V0
    Vec3 e2 = vec3_sub(tri->v2, tri->v0);   // E2 = V2 - V0

    Vec3   h = vec3_cross(ray.dir, e2);      // h = D × E2
    double a = vec3_dot(e1, h);              // a = E1 · h

    // 若 |a| ≈ 0，光線平行三角形平面 → 無交點
    if (fabs(a) < EPSILON) return 0;

    double f = 1.0 / a;                      // f = 1/a（共用除數）
    Vec3   s = vec3_sub(ray.origin, tri->v0); // S = O - V0
    double u = f * vec3_dot(s, h);           // u = (S · h) / a

    if (u < 0.0 || u > 1.0) return 0;       // u ∉ [0,1] → 在三角形外

    Vec3   q = vec3_cross(s, e1);            // q = S × E1
    double v = f * vec3_dot(ray.dir, q);     // v = (D · q) / a

    if (v < 0.0 || u + v > 1.0) return 0;   // v < 0 或 u+v > 1 → 在外

    double t = f * vec3_dot(e2, q);          // t = (E2 · q) / a
    if (t < t_min || t > t_max) return 0;   // t 不在有效範圍內

    // 確保法線指向光線來源側（雙面三角形）
    Vec3 n = tri->normal;
    if (vec3_dot(ray.dir, n) > 0.0) n = vec3_negate(n);

    rec->t = t;
    rec->point  = ray_at(ray, t);
    rec->normal = n;
    rec->mat    = tri->mat;
    return 1;
}
```

**為什麼這個演算法高效？**

總共只需要：1 次叉積 + 1 次點積計算 $a$，再 2 次叉積 + 3 次點積計算 $u, v, t$。整個過程沒有除法（除了 `1/a` 一次，之後都是乘法），沒有平方根，是目前最快的三角形求交方法之一。

#### 兩種方法的完整對比

| 項目 | 平面法（第一版） | Möller–Trumbore |
|------|----------------|-----------------|
| 向量運算次數 | 9 次 | 7 次 |
| 除法次數 | 2 次（$t$ + 邊測試正規化） | 1 次（`1/a`，之後全是乘法） |
| 平方根 | 1 次（法線 normalize） | 0 次 |
| 需要預存法線 | 是（`tri->normal`） | 否（但我們仍存，用於光照） |
| 輸出重心座標 | 否 | 是（$u, v$ 可用於 UV 插值） |
| 程式碼行數 | 約 20 行 | 約 15 行 |

兩者在**數學上完全等價**，結果相同。差別純粹在計算效率與中間資訊的豐富度。
我們的程式最終採用 Möller–Trumbore，但保留了 `tri->normal`（`triangle_init()` 預計算），
因為光照計算（第 4 章的 Phong shading）仍然需要法線。

---

## 3.3 球形求交：二次方程式

### 3.3.1 球面方程式

球心 $\vec{C}$、半徑 $r$ 的球面上任意一點 $\vec{P}$ 滿足：

$$|\vec{P} - \vec{C}|^2 = r^2$$

### 3.3.2 代入光線

令 $\vec{P}(t) = \vec{O} + t\vec{D}$ 代入：

$$|\vec{O} + t\vec{D} - \vec{C}|^2 = r^2$$

令 $\vec{oc} = \vec{O} - \vec{C}$，展開：

$$|t\vec{D} + \vec{oc}|^2 = r^2$$

$$t^2|\vec{D}|^2 + 2t(\vec{D} \cdot \vec{oc}) + |\vec{oc}|^2 - r^2 = 0$$

因為 $|\vec{D}| = 1$（已正規化），所以 $|\vec{D}|^2 = 1$：

$$t^2 + 2t(\vec{D} \cdot \vec{oc}) + (|\vec{oc}|^2 - r^2) = 0$$

令 $b = \vec{D} \cdot \vec{oc}$，$c = |\vec{oc}|^2 - r^2$，得標準二次方程式：

$$t^2 + 2bt + c = 0$$

### 3.3.3 判別式

$$\Delta = b^2 - c$$

| $\Delta$ | 意義 | 交點數 |
|----------|------|--------|
| $< 0$ | 光線不碰球 | 0 |
| $= 0$ | 光線切球（僅切一點） | 1 |
| $> 0$ | 光線穿過球 | 2 |

兩個解：

$$t_{1,2} = -b \pm \sqrt{\Delta}$$

取較小的正值（離相機更近的交點）。

### 3.3.4 代碼對照

```c
// include/scene.h — sphere_intersect()
static inline int sphere_intersect(const Sphere *s, Ray ray,
                                   double t_min, double t_max,
                                   HitRecord *rec)
{
    Vec3   oc   = vec3_sub(ray.origin, s->center);  // oc = O - C
    double b    = vec3_dot(ray.dir, oc);             // b = D · oc
    double c    = vec3_dot(oc, oc) - s->radius * s->radius;  // c = |oc|² - r²
    double disc = b * b - c;                         // Δ = b² - c
    if (disc < 0.0) return 0;                        // 不相交

    double sq = sqrt(disc);
    double t  = -b - sq;                             // 先嘗試較近的解
    if (t < t_min || t > t_max) {
        t = -b + sq;                                  // 再試較遠的解
        if (t < t_min || t > t_max) return 0;
    }

    Vec3 p = ray_at(ray, t);
    Vec3 n = vec3_normalize(vec3_sub(p, s->center)); // 法線 = (P - C) / r
    if (vec3_dot(ray.dir, n) > 0.0) n = vec3_negate(n);

    rec->t = t; rec->point = p; rec->normal = n; rec->mat = s->mat;
    return 1;
}
```

### 3.3.5 球面法線的幾何意義

$$\hat{N} = \frac{\vec{P} - \vec{C}}{r}$$

這個向量從球心指向交點，天然就是球面的法線方向，無需額外計算。長度正好是 $r / r = 1$，是單位向量。

---

## 3.4 圓錐求交：解析幾何

圓錐是三者中數學最複雜的，需要分開處理「側面」和「底面圓盤」。

### 3.4.1 圓錐面的隱式方程式

定義：頂點在 $\text{apex}$，軸朝 $-Y$ 方向，底面在 $y = \text{base\_y}$，底面半徑 $r$，高度 $H = \text{apex.y} - \text{base\_y}$。

令 $k = r / H$（半錐角的正切值，$\tan\alpha = k$），在以 apex 為原點的局部座標系中：

$$x^2 + z^2 = k^2 y^2, \quad -H \leq y \leq 0$$

這個方程式描述了**無限雙錐面**，我們用 $-H \leq y \leq 0$ 截取上半錐（頂點在 $y=0$，底面在 $y = -H$）。

**直覺理解：** 在高度 $y$（負值），水平截面半徑 $= k|y|$，離頂點越遠，截面越大。

### 3.4.2 代入光線

光線在局部座標系中（令 $\vec{oc} = \vec{O} - \text{apex}$）：

$$\vec{P}'(t) = \vec{oc} + t\vec{D}$$

代入圓錐面方程式：

$$(o_x + t d_x)^2 + (o_z + t d_z)^2 = k^2(o_y + t d_y)^2$$

展開整理（合併同次項）：

$$\underbrace{(d_x^2 + d_z^2 - k^2 d_y^2)}_{A} \cdot t^2 + 2\underbrace{(o_x d_x + o_z d_z - k^2 o_y d_y)}_{B} \cdot t + \underbrace{(o_x^2 + o_z^2 - k^2 o_y^2)}_{C} = 0$$

### 3.4.3 求解步驟

$$\Delta = B^2 - AC$$

兩個候選解：

$$t = \frac{-B \pm \sqrt{\Delta}}{A}$$

對每個候選 $t$，還需要**驗證是否在錐體高度範圍內**：

$$\vec{P}'_y = o_y + t \cdot d_y \in [-H, 0]$$

否則這個 $t$ 是圓錐面延伸部分（底部以下或頂點以上），不是我們想要的。

### 3.4.4 圓錐側面的法線

圓錐面 $F(x,y,z) = x^2 + z^2 - k^2 y^2 = 0$ 的梯度向量即為法線：

$$\nabla F = \left(\frac{\partial F}{\partial x}, \frac{\partial F}{\partial y}, \frac{\partial F}{\partial z}\right) = (2x,\ -2k^2 y,\ 2z)$$

正規化後（省略 $2$ 因子）：

$$\hat{N} = \text{normalize}(x,\ -k^2 y,\ z)$$

在局部座標系的交點 $(p_x, p_y, p_z)$ 處：

$$\text{lateral} = \sqrt{p_x^2 + p_z^2}$$

可以將法線改寫為更物理直覺的形式：

$$\hat{N} = \text{normalize}(p_x,\ \text{lateral} \cdot k,\ p_z)$$

**直覺：** $Y$ 方向分量為正（朝上），說明側面法線沿圓錐表面向外傾斜，$k$ 越大（錐角越大），法線越偏向水平方向。

### 3.4.5 底面圓盤求交

底面是一個水平圓盤，平面方程式：$y = \text{base\_y}$，圓盤半徑 $r$。

光線與平面 $y = \text{base\_y}$ 的交點：

$$t_{\text{disk}} = \frac{\text{base\_y} - O_y}{D_y}, \quad (D_y \neq 0)$$

再驗證交點是否在圓盤半徑內：

$$(P_x - \text{apex.x})^2 + (P_z - \text{apex.z})^2 \leq r^2$$

底面的法線固定為 $\hat{N} = (0, -1, 0)$（朝下），對應材質使用 `base_mat`。

### 3.4.6 代碼對照

```c
// include/scene.h — cone_intersect()（側面部分）
double H  = cone->apex.y - cone->base_y;
double k  = cone->radius / H;
double k2 = k * k;

Vec3   oc = vec3_sub(ray.origin, cone->apex);
double dx = ray.dir.x, dy = ray.dir.y, dz = ray.dir.z;
double ox = oc.x,      oy = oc.y,      oz = oc.z;

// 二次方程式係數
double A =  dx*dx + dz*dz - k2*dy*dy;
double B =  ox*dx + oz*dz - k2*oy*dy;   // B/2 形式
double C =  ox*ox + oz*oz - k2*oy*oy;

double disc = B*B - A*C;               // Δ = B² - AC
if (disc >= 0.0) {
    double sq = sqrt(disc);
    double roots[2] = { (-B - sq) / A, (-B + sq) / A };
    for (int i = 0; i < 2; ++i) {
        double t = roots[i];
        if (t < t_min || t >= best_t) continue;

        Vec3   p  = ray_at(ray, t);
        double py = p.y - cone->apex.y;          // 局部座標 y 分量
        if (py > EPSILON || py < -H - EPSILON) continue;  // 範圍檢查

        // 計算側面法線
        double px  = p.x - cone->apex.x;
        double pz  = p.z - cone->apex.z;
        double lat = sqrt(px*px + pz*pz);        // 水平距離
        Vec3 n = vec3_normalize(vec3(px, lat*k, pz));
        if (vec3_dot(ray.dir, n) > 0.0) n = vec3_negate(n);

        best_t = t;
        br = (HitRecord){ t, p, n, cone->side_mat };
        hit = 1;
    }
}
```

---

## 3.5 `t_min` 與 `t_max`：避免自交

在 `scene_hit()` 中，我們總是用 `t_min = 1e-4` 而非 `0`：

```c
// src/renderer.c — scene_hit()
if (object_intersect(&scene->objects[i], ray, 1e-4, closest, &tmp))
```

**原因：** 浮點數精度問題。

當光線從交點 $P$ 射出（用於陰影偵測），起點就是 $P$ 本身。但 $P$ 是由 `ray_at(ray, t)` 計算得到的，浮點誤差使 $P$ 可能略微偏在幾何體表面的錯誤側，導致光線立刻與同一個物體相交（$t \approx 0$），產生「自交（self-intersection）」，出現錯誤的黑色陰影點。

解決方法：設 `t_min = 1e-4`，忽略起點附近極小的 $t$ 值。

```c
// renderer.c — phong_shade()
Vec3 shadow_origin = vec3_add(rec->point, vec3_scale(N, 1e-4));
// 沿法線方向偏移起點，再加上 t_min 的雙重保護
Ray shadow_ray = ray_make(shadow_origin, L);
```

---

## 3.6 最近交點的選擇

場景中有多個物體，要找**最近**的交點：

```c
// src/renderer.c — scene_hit()
static int scene_hit(const Scene *scene, Ray ray,
                     double t_min, double t_max, HitRecord *rec)
{
    HitRecord tmp;
    int    hit_any = 0;
    double closest = t_max;         // 當前最近的 t

    for (int i = 0; i < scene->obj_count; ++i) {
        // 每次將 t_max 設為目前最近的 t
        // 這樣更遠的物體就自動被排除在外
        if (object_intersect(&scene->objects[i], ray, t_min, closest, &tmp)) {
            hit_any = 1;
            closest = tmp.t;        // 更新最近距離
            *rec    = tmp;          // 記錄最近的交點
        }
    }
    return hit_any;
}
```

這個「逐漸縮小 `t_max`」的技巧讓我們在一次掃描中找到最近交點，不需要先收集所有交點再排序。

---

## 3.7 本章總結

**三角形求交的演進脈絡：**

```
第一版（平面法）：
  光線 → 與平面求交得 t → 算出交點 P → 三邊叉積測試 → 判斷 P 在內/外

Möller–Trumbore：
  光線 → 聯立平面方程 + 重心座標 → 一次求解 (t, u, v) → 範圍判斷
  （把兩步合成一步，少一次除法、一次平方根）
```

| 幾何體 | 求交方法 | 關鍵數學 | 法線計算 |
|--------|---------|---------|---------|
| 三角形（第一版） | 平面求交 + 邊叉積測試 | 點法式平面方程 | 叉積（預計算，必須存） |
| 三角形（現行版） | Möller–Trumbore | 線性方程 Cramer's Rule | 叉積（預計算，光照用） |
| 球形 | 代入球面方程 | 二次方程式 | $(P - C) / r$ |
| 圓錐側面 | 代入圓錐面方程 | 二次方程式 + 範圍檢查 | 梯度向量 |
| 圓錐底面 | 與水平面求交 | 直接除法 + 圓內測試 | $(0, -1, 0)$ |

> **下一章：** [04 — 光照物理與 Phong 模型](./04_lighting.md)
> 找到交點之後，如何計算它的顏色？這需要物理光照模型。
