# 第 2 章：光線模型與 Pinhole Camera

> **本章對應代碼：** `include/ray.h`、`include/camera.h`
> **核心問題：** 如何用數學描述一條光線？相機如何把「像素座標」轉換成「空間中的光線」？

---

## 2.1 光線的數學表示

一條光線（Ray）在數學上是一條**參數方程式**描述的半直線：

$$\vec{P}(t) = \vec{O} + t\,\vec{D}, \quad t \geq 0$$

| 符號 | 意義 | 程式中 |
|------|------|--------|
| $\vec{O}$ | 光線起點（Origin） | `ray.origin` |
| $\vec{D}$ | 光線方向（Direction，單位向量） | `ray.dir` |
| $t$ | 「走了多遠」的參數 | 求交算出的 `t` 值 |
| $\vec{P}(t)$ | 光線上距起點 $t$ 處的空間點 | `ray_at(ray, t)` |

```c
// include/ray.h
typedef struct {
    Vec3 origin;
    Vec3 dir;       // 保證是單位向量（已 normalize）
} Ray;

static inline Vec3 ray_at(Ray r, double t) {
    // P(t) = O + t * D
    return vec3_add(r.origin, vec3_scale(r.dir, t));
}
```

### 為什麼 `dir` 必須是單位向量？

若 $|\vec{D}| = 1$，則 $t$ 的單位就是「場景空間的長度單位」：

$$|{\vec{P}(t) - \vec{O}}| = |t\vec{D}| = t|\vec{D}| = t$$

這讓 `t` 值直接等於光線走的距離，簡化光源距離衰減、陰影光線長度等計算。

```c
static inline Ray ray_make(Vec3 origin, Vec3 dir) {
    return (Ray){origin, vec3_normalize(dir)};
    //                   ↑ 強制正規化，保證 |dir| = 1
}
```

---

## 2.2 相機模型：Pinhole Camera

### 2.2.1 什麼是 Pinhole Camera？

Pinhole Camera（針孔相機）是最簡單的成像模型：

```
                     虛擬螢幕（Virtual Screen）
                    ┌─────────────────────┐
                    │  ·     ·     ·      │
光線 ←───────────────  ·     ·     ·      │
                    │  ·     ·     ·      │
                    └─────────────────────┘
                              ↑
                         focal_length = 1
                              ↑
                          [相機點]
```

- 相機是**一個點**（光圈無限小）
- 所有光線都從這個點出發
- 虛擬螢幕放在相機前方 `focal_length = 1` 處
- 每個像素對應螢幕上的一個小區域，射出一條光線

**優點：** 數學簡單，無需模擬光圈、鏡頭折射
**代價：** 景深無限，所有距離都清楚（真實相機有焦外模糊）

### 2.2.2 座標系建立

相機需要一個自己的座標系，描述「前後左右上下」方向：

**輸入：**
- `position`：相機位置
- `lookat`：看向的目標點
- `up`：世界的「上方」方向（通常 `(0,1,0)`）
- `fov_deg`：垂直視角（Field of View）
- `aspect`：寬高比（width / height）

**建立右手座標系：**

$$\vec{w} = \text{normalize}(\text{position} - \text{lookat}) \quad \text{（相機後方）}$$

$$\vec{u} = \text{normalize}(\vec{up} \times \vec{w}) \quad \text{（相機右方）}$$

$$\vec{v} = \vec{w} \times \vec{u} \quad \text{（相機上方）}$$

```c
// include/camera.h — camera_init()
Vec3 w = vec3_normalize(vec3_sub(position, lookat));  // 後方
Vec3 u = vec3_normalize(vec3_cross(up, w));           // 右方
Vec3 v = vec3_cross(w, u);                            // 上方
```

> **注意：** 我們用的是**右手座標系**。
> 若你面朝 $-\vec{w}$ 方向（即看向目標），$\vec{u}$ 在右，$\vec{v}$ 在上，$\vec{w}$ 在你背後。
> 叉積順序 `cross(up, w)` 而非 `cross(w, up)` 決定了 $\vec{u}$ 朝右而非朝左。

---

### 2.2.3 FOV 與虛擬螢幕尺寸

垂直視角 `fov_deg` 決定螢幕的半高度：

$$\text{half\_h} = \tan\!\left(\frac{\text{fov}}{2}\right)$$

$$\text{half\_w} = \text{aspect} \times \text{half\_h}$$

```
          半高 half_h
          ┌────────┐
          │        │ ← 虛擬螢幕
          │        │
          └────────┘
          ↑
       focal = 1
          ↑
       [相機點]

fov/2 角度 → tan(fov/2) = half_h / 1 → half_h = tan(fov/2)
```

```c
double theta  = fov_deg * M_PI / 180.0;
double half_h = tan(theta / 2.0);
double half_w = aspect * half_h;
```

---

### 2.2.4 螢幕的四個角落向量

確定螢幕大小後，計算左下角在世界座標中的位置：

$$\text{center} = \text{position} - \vec{w} \quad \text{（螢幕中心，沿 -w 走 focal\_length=1）}$$

$$\text{lower\_left} = \text{center} - \text{half\_w} \cdot \vec{u} - \text{half\_h} \cdot \vec{v}$$

```c
Vec3 center = vec3_sub(position, w);   // 螢幕中心

Vec3 lower_left = vec3_sub(
    vec3_sub(center, vec3_scale(u, half_w)),   // 往左移 half_w
    vec3_scale(v, half_h)                       // 往下移 half_h
);

// 螢幕的水平和垂直跨度向量
Vec3 horizontal = vec3_scale(u, 2.0 * half_w);  // 從左到右
Vec3 vertical   = vec3_scale(v, 2.0 * half_h);  // 從下到上
```

這四個向量（`lower_left`、`horizontal`、`vertical`、`position`）存在 `Camera` struct 裡，是之後每次生成光線的基礎。

---

## 2.3 像素座標轉光線

渲染時，對每個像素 `(i, j)` 生成一條光線：

1. 將像素座標**正規化**到 `[0, 1]` 範圍：

$$s = \frac{i}{W-1}, \quad t = \frac{j}{H-1}$$

2. 在螢幕上找到對應的世界座標點：

$$\vec{T} = \text{lower\_left} + s \cdot \text{horizontal} + t \cdot \text{vertical}$$

3. 從相機點射向螢幕點：

$$\vec{D} = \text{normalize}(\vec{T} - \text{position})$$

```c
// include/camera.h — camera_get_ray()
static inline Ray camera_get_ray(const Camera *cam, double s, double t) {
    Vec3 target = vec3_add(
        cam->lower_left,
        vec3_add(
            vec3_scale(cam->horizontal, s),   // 水平偏移
            vec3_scale(cam->vertical,   t)    // 垂直偏移
        )
    );
    Vec3 dir = vec3_sub(target, cam->position);
    return ray_make(cam->position, dir);      // 自動 normalize
}
```

**渲染主迴圈中的呼叫：**

```c
// src/renderer.c — render()
for (int j = height-1; j >= 0; --j) {       // 從上到下掃描
    for (int i = 0; i < width; ++i) {
        double u = (double)i / (width  - 1); // [0, 1]
        double v = (double)j / (height - 1); // [0, 1]（j 由大到小 → v 由大到小）

        Ray ray = camera_get_ray(cam, u, v);
        // ... 追蹤這條光線
    }
}
```

> **座標系的一個陷阱：** 螢幕座標 `j=0` 是最底行，但圖像記憶體通常從頂部開始儲存。
> 所以迴圈從 `j = height-1`（螢幕頂部）遞減到 `j = 0`（螢幕底部），
> 寫入 framebuffer 時做一次上下翻轉：`row = height - 1 - j`。

---

## 2.4 相機參數對畫面的影響

### FOV 的幾何效果

| FOV | `half_h = tan(fov/2)` | 螢幕寬度 | 效果 |
|-----|----------------------|---------|------|
| 30° | 0.268 | 窄 | 長焦，壓縮空間感 |
| 45° | 0.414 | 中 | 接近人眼自然感 |
| 60° | 0.577 | 寬 | 略有廣角感 |
| 90° | 1.000 | 很寬 | 明顯廣角變形 |

**數學上的極端情況：**
- FOV → 0°：平行投影（Orthographic），所有光線平行，無透視感，工程製圖用
- FOV → 180°：整個半球都能看到，極端魚眼效果

### 相機距離與 FOV 的關係

「長焦效果」不需要改變投影方式，只需要：

$$\text{拉遠相機距離} \times \text{縮小 FOV}$$

兩者的組合保持物體在畫面中的大小不變，但透視壓縮感（近大遠小的幅度）隨距離增大而減小。

```c
// 電影感長焦設定
Vec3 cam_pos = {0.0, 3.0, 12.0};   // 相機拉遠一倍
double fov   = 30.0;               // FOV 縮小一半
// → 物體看起來一樣大，但形體比例更自然

// 廣角近拍設定
Vec3 cam_pos = {0.0, 1.5, 4.0};    // 相機推近
double fov   = 75.0;               // 寬 FOV
// → 強烈透視感，邊角物體明顯拉扯
```

---

## 2.5 本章總結

```
像素座標 (i, j)
      ↓  正規化
螢幕座標 (s, t) ∈ [0,1]²
      ↓  線性插值
螢幕世界點 T = lower_left + s·horizontal + t·vertical
      ↓  相減正規化
光線方向 D = normalize(T - position)
      ↓  打包
Ray { origin = position, dir = D }
      ↓
進入第 3 章：與場景幾何體求交
```

**Camera struct 儲存的四個向量：**

| 欄位 | 意義 |
|------|------|
| `position` | 相機點（所有光線的起點） |
| `lower_left` | 螢幕左下角世界座標 |
| `horizontal` | 螢幕水平跨度向量（從左到右） |
| `vertical` | 螢幕垂直跨度向量（從下到上） |

> **下一章：** [03 — 幾何求交：三角形、球、圓錐](./03_intersection.md)
> 光線射出後，它碰到了什麼？這是整個 Ray Tracer 數學最密集的部分。
