# c11-raytracer-fundamentals — 擴展路線圖

> 本文件整理從基礎實作出發的所有擴展方向，
> 每個里程碑都是獨立可交付的，可以按需選擇實作順序。

---

## 目前狀態（基準線）

```
幾何體   Triangle / Sphere / Cone（含底面圓盤）
光照     Phong（環境光 + Lambert 漫射 + Phong 高光）
陰影     硬陰影（點光源 Shadow Ray）
相機     Pinhole，可控 position / lookat / FOV
輸出     PNG（Gamma 2.2，zlib 內建編碼）
抗鋸齒   Jitter 超採樣
測試     TDD，69 個單元測試（CTest）
```

---

## 里程碑一：基礎幾何體補完

> **目標：** 讓場景能組合出更多形狀，全部使用現有的 `Object` tagged union 架構，不需改動渲染器。

### M1-A　解析幾何族（難度 ★☆☆）

這三個幾何體的求交數學與現有的 Sphere / Cone 完全同族，
實作成本最低，一個下午可以全部完成。

| 幾何體 | 隱式方程式 | 法線 | 備註 |
|--------|-----------|------|------|
| **Plane**（無限平面） | $\hat{N} \cdot (\vec{P} - P_0) = 0$ | 固定 $\hat{N}$ | 適合做地板、牆壁、鏡面 |
| **Cylinder**（圓柱體） | $x^2 + z^2 = r^2$，$y \in [y_0, y_1]$ | $(p_x, 0, p_z) / r$ | 上下蓋重用 Disk 邏輯 |
| **Disk**（有限圓盤） | 平面求交 $+$ $\|P - C\|^2 \leq r^2$ | 與平面同 | 從 Cone 底面獨立出來 |

**架構影響：**
```c
// scene.h 只需在 GeomType 新增三個值
typedef enum {
    GEOM_TRIANGLE, GEOM_SPHERE, GEOM_CONE,
    GEOM_PLANE,    // 新增
    GEOM_CYLINDER, // 新增
    GEOM_DISK,     // 新增
} GeomType;
```

---

### M1-B　Slab 族（難度 ★★☆）

| 幾何體 | 求交方法 | 備註 |
|--------|---------|------|
| **AABB / Cube** | Slab method：三對軸平行平面，各求 $t$ 區間後取交集 | Cube = AABB 的特例；也是 BVH 加速的基礎節點 |
| **OBB**（任意方向包圍盒） | 先將光線旋轉到盒子局部座標，再做 AABB | 比 AABB 多一個 $3 \times 3$ 旋轉矩陣 |
| **Rect**（有限矩形） | 平面求交後，測試 UV 是否在 $[0,w] \times [0,h]$ | 天然支援 UV 貼圖（紋理的前置步驟） |

**Slab Method 核心：**
```c
double tx0 = (aabb.min.x - ray.origin.x) / ray.dir.x;
double tx1 = (aabb.max.x - ray.origin.x) / ray.dir.x;
// 對 y, z 重複，然後：
double t_enter = max3(min(tx0,tx1), min(ty0,ty1), min(tz0,tz1));
double t_exit  = min3(max(tx0,tx1), max(ty0,ty1), max(tz0,tz1));
// t_enter <= t_exit → 擊中
```

---

### M1-C　高難度解析幾何（難度 ★★★）

| 幾何體 | 求交方法 | 挑戰 |
|--------|---------|------|
| **Torus**（圓環體） | 四次方程式（需解四根） | 目前架構只處理二次方程，需擴充根求解器 |
| **Paraboloid**（拋物面） | 二次方程 + 高度截斷 | 和 Cone 類似但係數不同 |
| **Hyperboloid**（雙曲面） | 二次方程 + 範圍截斷 | 可做沙漏型幾何體 |

---

## 里程碑二：CSG 布林運算

> **目標：** 讓幾何體可以做聯集、差集、交集，實現「挖洞」、「截面」等效果。

### 概念

CSG（Constructive Solid Geometry）不是新幾何體，而是**架構層的改動**。

目前 `object_intersect()` 只回傳「最近一個交點」，
CSG 需要回傳「光線穿越這個幾何體的所有 $t$ 區間列表」：

```
球體的區間：  ──[══════]──  （t_enter=2, t_exit=6）
方塊的區間：  ────[════════]  （t_enter=4, t_exit=10）

Union  A∪B：  ──[══════════]  [2, 10]
Subtract A-B： ──[══]──────    [2, 4]
Intersect A∩B：────[════]──    [4, 6]
```

### 需要的架構改動

```c
// 新增：區間型別
typedef struct {
    double   t_enter, t_exit;
    Vec3     n_enter, n_exit;
    Material mat;
} Interval;

// 新增：CSG 節點
typedef enum { CSG_UNION, CSG_SUBTRACT, CSG_INTERSECT } CsgOp;
typedef struct CsgNode {
    CsgOp   op;
    Object *left;
    Object *right;
} CsgNode;

// object_intersect() 需要擴充為回傳區間列表
// 這是本專案最大的一次架構重構
```

### 視覺效果

| 操作 | 效果 | 應用 |
|------|------|------|
| `A ∪ B` | 兩個形狀合成一個 | 複合形狀 |
| `A − B` | 從 A 挖去 B | 挖洞、截面 |
| `A ∩ B` | 只保留重疊部分 | 截取特定區域 |

---

## 里程碑三：三角網格（OBJ 載入）

> **目標：** 載入 `.obj` 檔案，渲染任意 3D 模型。

### 流程

```
.obj 檔案
   ↓ 解析頂點 v / 面 f
Triangle[] 陣列（可能數萬個三角形）
   ↓ 直接用現有 MT 求交（但速度不可接受）
   ↓ 建立 BVH
O(log N) 求交
```

### OBJ 格式（只需解析基本子集）

```
v  1.0  0.0  0.0    ← 頂點座標
vn 0.0  1.0  0.0    ← 頂點法線（可選）
vt 0.5  0.5         ← UV 座標（可選）
f  1 2 3            ← 面（三角形頂點索引）
f  1/1/1 2/2/2 3/3/3  ← 面（頂點/UV/法線 索引）
```

---

## 里程碑四：BVH 加速結構

> **目標：** 讓大場景（1000+ 物件）的渲染速度從 $O(N)$ 降至 $O(\log N)$。

### 原理

```
目前：每條光線 × 每個物件 = O(N) 次求交
BVH： 每條光線先與 AABB 包圍盒測試，跳過大量不可能相交的物件
      最終複雜度 O(log N)
```

### 建構策略

**SAH（Surface Area Heuristic）** 是業界標準的 BVH 分割策略：

$$\text{cost}(\text{split}) = C_t + \frac{A_L}{A_P} \cdot N_L \cdot C_i + \frac{A_R}{A_P} \cdot N_R \cdot C_i$$

其中 $A_L, A_R$ 是左右子節點的包圍盒表面積，$N_L, N_R$ 是物件數量。

```c
typedef struct BVHNode {
    AABB          bounds;
    struct BVHNode *left, *right;
    Object        *leaf_obj;   /* 葉節點才有值 */
} BVHNode;
```

**建議實作順序：** 先用最簡單的「沿最長軸從中間切」策略驗證正確性，再升級到 SAH。

---

## 里程碑五：物理基礎渲染（PBR）

> **目標：** 用物理正確的材質模型替換 Phong，與現代遊戲引擎的材質系統對齊。

### Phong vs PBR

| 項目 | Phong（現有） | PBR（Cook-Torrance） |
|------|-------------|---------------------|
| 基礎 | 經驗公式 | 物理 BRDF |
| 材質參數 | ambient / diffuse / specular / shininess | albedo / metallic / roughness |
| 能量守恆 | 否 | 是 |
| Fresnel | 無 | Schlick 近似 |
| 法線分佈 | $(R \cdot V)^n$ | GGX |

### Cook-Torrance BRDF

$$f_r = \frac{D(\vec{h}) \cdot F(\vec{v}, \vec{h}) \cdot G(\vec{l}, \vec{v}, \vec{h})}{4(\vec{n} \cdot \vec{l})(\vec{n} \cdot \vec{v})}$$

- $D$：GGX 法線分佈函數
- $F$：Schlick Fresnel 近似
- $G$：Smith 幾何遮蔽函數

---

## 里程碑六：路徑追蹤（Path Tracing）

> **目標：** 從 Phong/PBR 的直接光照升級到全域光照，支援間接照明、軟陰影、焦散。

### 核心差異

```
現有（直接光照）：
  交點 → Shadow Ray → 光源
  只計算光源的直接貢獻

路徑追蹤：
  交點 → 隨機反彈方向 → 再次求交 → 再次反彈 ...
  → 最終打到光源（或超過最大深度截止）
  計算所有間接光照
```

### 蒙地卡羅積分

$$L_o(\vec{x}, \vec{\omega}_o) = \int_\Omega f_r(\vec{x}, \vec{\omega}_i, \vec{\omega}_o) L_i(\vec{x}, \vec{\omega}_i) (\hat{n} \cdot \vec{\omega}_i) \, d\vec{\omega}_i$$

用蒙地卡羅方法估計：每像素發射大量光線（512–4096 spp），取平均。

### 實作步驟

1. `trace()` 改為遞迴，加入 `max_depth` 參數
2. 在交點以餘弦分佈隨機取樣反彈方向
3. 加入 Russian Roulette 提前終止（避免無限遞迴）
4. 超採樣數從 8 提高到 512+（降噪是必要步驟）

---

## 里程碑七：場景描述語言

> **目標：** 讓場景定義從 C 硬編碼改為外部檔案，支援不重新編譯就能更換場景。

### 方案比較

| 方案 | 優點 | 缺點 | 建議 |
|------|------|------|------|
| **YAML** | 人類可讀、有成熟的解析庫 | 需要引入外部依賴（`libyaml`） | 適合「使用者友善」優先 |
| **JSON** | 標準化、解析庫輕量 | 不支援注解，數字冗長 | 適合與工具鏈整合 |
| **自訂 DSL** | 零依賴、語法可為渲染器量身設計 | 需要自己寫 lexer/parser | 適合「學習」優先 |
| **TOML** | 比 YAML 簡單，支援注解 | 解析庫相對少見 | 折衷選擇 |

---

### 方案 A：YAML 場景描述

```yaml
# scene.yaml
camera:
  position: [0.0, 3.0, 8.0]
  lookat:   [0.0, 1.0, 0.0]
  fov:      45.0

lights:
  - type: point
    position: [4.0, 7.0, 5.0]
    color:    [1.4, 1.3, 1.1]

objects:
  - type: cone
    apex:    [0.0, 2.5, 0.0]
    base_y:  0.0
    radius:  1.0
    material:
      diffuse:   [0.90, 0.35, 0.08]
      specular:  [1.00, 0.80, 0.40]
      shininess: 120.0

  - type: sphere
    center: [1.8, 0.9, 0.0]
    radius: 0.9
    material:
      diffuse:   [0.10, 0.35, 0.75]
      specular:  [0.95, 0.95, 1.00]
      shininess: 256.0
```

**依賴：** `libyaml`（C 語言，MIT 授權，單檔可嵌入）

---

### 方案 B：JSON 場景描述

```json
{
  "camera": {
    "position": [0.0, 3.0, 8.0],
    "lookat":   [0.0, 1.0, 0.0],
    "fov":      45.0
  },
  "lights": [
    {
      "type":     "point",
      "position": [4.0, 7.0, 5.0],
      "color":    [1.4, 1.3, 1.1]
    }
  ],
  "objects": [
    {
      "type":   "sphere",
      "center": [1.8, 0.9, 0.0],
      "radius": 0.9,
      "material": {
        "diffuse":   [0.10, 0.35, 0.75],
        "specular":  [0.95, 0.95, 1.00],
        "shininess": 256.0
      }
    }
  ]
}
```

**依賴：** `cJSON`（純 C，MIT 授權，單一 `.c` + `.h` 可直接嵌入，零其他依賴）

---

### 方案 C：自訂 DSL（零依賴）

設計一個極簡的 S-expression 風格語法，只需約 300 行 C 就能完成 lexer + parser：

```
# scene.rt  — 自訂 DSL 範例

camera
  position  0 3 8
  lookat    0 1 0
  fov       45

light point
  position  4 7 5
  color     1.4 1.3 1.1

cone
  apex      0 2.5 0
  base_y    0
  radius    1
  diffuse   0.9 0.35 0.08
  specular  1.0 0.8  0.4
  shininess 120

sphere
  center    1.8 0.9 0
  radius    0.9
  diffuse   0.1  0.35 0.75
  specular  0.95 0.95 1.0
  shininess 256
```

**Parser 結構：**
```c
// 約 300 行實作
typedef enum { TOK_IDENT, TOK_NUMBER, TOK_NEWLINE, TOK_EOF } TokenType;
typedef struct { TokenType type; char str[64]; double num; } Token;

Scene *scene_load_rt(const char *path);   // 解析 .rt 檔
```

---

### 建議：採用 JSON + cJSON

對這個專案而言，JSON + cJSON 是最平衡的選擇：

- **零新增的系統依賴**：`cJSON.c` + `cJSON.h` 直接放進 `src/` 或 `third_party/`
- **生態系統廣泛**：任何語言都能生成 JSON，方便未來做場景編輯器
- **學習價值高**：JSON 解析是實際工程中的常見課題
- **CMakeLists.txt 幾乎不需改動**：只多一個 `cJSON.c` 編譯單元

**整合方式：**
```cmake
# CMakeLists.txt 只需加這一行
set(SOURCES src/main.c src/renderer.c src/scene_loader.c third_party/cJSON.c)
```

**新的 main 用法：**
```c
// 原本：場景硬編碼在 main.c
// 改後：
Scene *scene = scene_load_json("scene.json");
render(&cam, scene, width, height, samples, fb);
```

---

## 擴展優先順序建議

```
現在可以做（低風險、高價值）
  M1-A  Plane + Cylinder + Disk       ← 一個下午
  M1-B  AABB / Cube                   ← 半天，也是 BVH 前置步驟
  M7    JSON 場景描述（cJSON）         ← 兩天，大幅改善使用體驗

下一步（中等複雜度）
  M1-C  Torus                         ← 四次方程式，數學有挑戰性
  M4    BVH 加速結構                   ← 必須在 OBJ 載入前完成
  M3    OBJ 載入                       ← 依賴 BVH

進階（大幅重構）
  M2    CSG 布林運算                   ← 需要改動 object_intersect() 介面
  M5    PBR 材質                       ← 替換 Phong，材質系統重寫
  M6    路徑追蹤                        ← trace() 架構改為遞迴，需大量 spp
```

---

## 文件擴充計畫

每個里程碑完成後，對應補充教學文件：

| 文件 | 對應里程碑 |
|------|-----------|
| `docs/06_new_geometries.md` | M1-A / M1-B |
| `docs/07_csg.md` | M2 |
| `docs/08_bvh.md` | M4 |
| `docs/09_pbr.md` | M5 |
| `docs/10_path_tracing.md` | M6 |
| `docs/11_scene_format.md` | M7 |

---

*本路線圖是活文件，隨實作進展持續更新。*
*每個里程碑完成後建議獨立 commit，並加入對應的單元測試。*
