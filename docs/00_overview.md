# Ray Tracer 從零開始：教學系列總覽

> 本系列以我們實作的 C11 Ray Tracer 為主軸，由淺入深講解背後的數學、物理與程式設計。
> 每篇文件都會將**理論公式**與**實際代碼**對照，讓你知道「每一行在算什麼」。

---

## 文件地圖

```
docs/
├── 00_overview.md          ← 你在這裡：總覽與學習路線
├── 01_vector_math.md       ← 向量代數：Ray Tracer 的語言
├── 02_ray_and_camera.md    ← 光線模型與 Pinhole Camera
├── 03_intersection.md      ← 幾何求交：三角形、球、圓錐
├── 04_lighting.md          ← 光照物理與 Phong 模型
└── 05_rendering.md         ← 渲染管線、超採樣與 Gamma 校正
```

---

## 什麼是 Ray Tracing？

Ray Tracing（光線追蹤）是一種**逆向光學模擬**。

真實世界的光從光源出發，經過無數次反彈，最終有極少數光子進入眼睛。
正向模擬這個過程極度浪費——大多數光子根本不會碰到相機。

Ray Tracing 的天才之處在於**反轉方向**：

```
真實光路：  光源 → [無數反彈] → 眼睛（攝影機）
Ray Tracing：眼睛 → [追蹤路徑] → 光源
```

從每個像素射出一條「問詢光線」，問它最終會碰到什麼、受到哪些光源照射。
這樣就只計算「對最終影像有貢獻」的光路，效率大幅提升。

### 演算法最高層俯視

```
對每個像素 (i, j)：
  1. 從相機射出光線 ray
  2. 找到 ray 與場景中最近的交點 hit
  3. 計算 hit 點受到的光照 → 得到顏色
  4. 把顏色寫入 framebuffer
輸出 framebuffer → 圖像
```

這五行就是整個 `renderer.c` 的靈魂。後面的章節，每一章都在深挖其中一個步驟。

---

## 我們的程式架構

```
.
├─ include/
│   ├── vec3.h        → 第 1 章：向量數學
│   ├── ray.h         → 第 2 章：光線
│   ├── camera.h      → 第 2 章：相機
│   ├── scene.h       → 第 3 章：幾何求交
│   └── renderer.h    → 第 4、5 章：光照 + 渲染管線
└─ src/
    ├── renderer.c    → 第 4、5 章
    └── main.c        → 場景定義
```

每個 `.h` 檔都是獨立的知識單元，零外部依賴（除了 `<math.h>`）。

---

## 先備知識

| 主題 | 需要程度 |
|------|----------|
| C 語言基礎 | 熟悉 struct、指標、static inline |
| 高中向量 | 會加減乘、知道點積概念即可 |
| 高中物理 | 反射定律、光的直線傳播 |
| 座標系 | 理解 XYZ 三維座標 |

不需要大學微積分——我們用到的地方會從頭解釋。

---

## 建議學習順序

```
第 1 章（向量）→ 第 2 章（光線+相機）→ 第 3 章（求交）
                                              ↓
                               第 5 章（渲染管線）← 第 4 章（光照）
```

第 3 章是最數學密集的部分，建議搭配程式碼逐行閱讀。
第 4 章是最有「物理感」的部分，建議先理解直覺再看公式。

---

## 快速建置與執行

```bash
git clone https://github.com/KunYi/c11-raytracer-fundamentals.git raytracer
cd raytracer
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
./raytracer          # 輸出 view_0.png … view_5.png
```

用任何圖像軟體（Preview、GIMP、IrfanView）開啟 `.png` 即可看到渲染結果。

---

*下一篇：[01 — 向量數學：Ray Tracer 的語言](./01_vector_math.md)*
