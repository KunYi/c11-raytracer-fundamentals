# Golden Prompt：C11 Ray Tracer 從零到完整實作

> 這份文件是一個**可重現的學習指南**。  
> 你可以把以下的 Prompt 貼給任何支援程式開發的 AI（Claude、GPT-4 等），  
> 它會引導你從零開始，走到一個完整的 Ray Tracer 實作，並附帶深度教學文件。
>
> **本專案的完整代碼與文件在：**  
> 👉 `https://github.com/你的帳號/c11-raytracer-fundamentals`

---

## 如何使用這份文件

1. 複製下方 **「主 Prompt」** 貼給 AI，開啟新對話
2. AI 會建立基礎架構，你照著指示建置、執行
3. 之後按照 **「延伸 Prompt 系列」** 一步步擴充功能
4. 遇到不懂的地方，用 **「提問模板」** 問 AI

每個 Prompt 都是獨立的，但建議按順序走，因為每步都建立在上一步的基礎上。

---

## 主 Prompt（複製這段開始）

```
你是世界頂級的 C11 資深程式員，同時熟知電腦圖學、物理光學與線性代數。

請幫我用 CMake + C11 實作一個基礎的 Ray Tracer，要求如下：

【技術規格】
- 語言：C11，標準函式庫只用 <math.h>、<stdio.h>、<stdlib.h>、<string.h>
- 建置：CMake 3.15+，Release 模式帶 -O3 -ffast-math
- 輸出：PPM P6 格式（可用任何圖像軟體開啟）

【場景內容】
- 一個點光源（Point Light）
- 一個圓錐體（Cone，解析幾何求交，含底面圓盤）
- 一個球形（Sphere，二次方程式求交）
- 一個地板（兩個三角形拼成的平面）

【相機系統】
- Pinhole Camera（針孔相機，理想鏡頭）
- 可控制：position、lookat、fov（垂直視角）
- 右手座標系

【光照模型】
- Phong Shading：環境光 + 漫射（Lambert）+ 高光（Phong specular）
- 硬陰影（Shadow Ray）
- 點光源二次衰減：att = 1 / (1 + kl*d + kq*d²)
- Gamma 2.2 校正輸出

【渲染功能】
- Jitter 超採樣抗鋸齒（可設定 samples 數）
- 批次渲染 6 個不同相機角度

【程式架構】
請設計成以下分層結構，每個檔案職責單一：
- vec3.h     — 向量數學（全部 static inline，零外部依賴）
- ray.h      — 光線結構
- camera.h   — Pinhole Camera
- scene.h    — 幾何體（Triangle/Sphere/Cone）+ 統一 Object 介面
- renderer.h — 渲染器介面
- renderer.c — Phong shading + 渲染主迴圈 + PPM 輸出
- main.c     — 場景定義 + 6 個相機視角批次渲染

【代碼要求】
- 每個函式都要有清楚的中文或英文註解，說明它在做什麼數學/物理
- scene.h 使用 tagged union（GeomType enum + union）實作多型
- 所有向量運算使用 static inline 確保零開銷

請先建立完整的專案結構，並確認可以編譯執行，輸出 6 張不同角度的 PPM 圖像。
```

---

## 延伸 Prompt 系列

執行主 Prompt 並確認渲染成功後，用以下 Prompt 一步步深化。

---

### Prompt 2：理解透視變形

```
我發現渲染結果在某些角度有「變形感」。
請深入解釋：
1. 我們的 Pinhole Camera 是理想鏡頭嗎？它有光學畸變嗎？
2. 為什麼理想鏡頭也會產生「變形感」？請從幾何原理說明
3. 真實的遊戲引擎（如 Unreal、Unity）和電影 CG（如皮克斯）
   是如何處理這個問題的？
4. 在我們現有的代碼中，哪些相機參數組合可以得到最自然的比例感？

請搭配示意圖或公式解說，不只是文字描述。
```

---

### Prompt 3：深度教學文件

```
請為這個 Ray Tracer 專案撰寫一套完整的 Markdown 教學文件系列，
要求放在 docs/ 資料夾，包含以下 6 篇：

docs/00_overview.md     — 總覽、學習地圖、演算法高層俯視
docs/01_vector_math.md  — 向量數學：加減、點積、叉積、正規化、反射向量
docs/02_ray_and_camera.md — 光線模型與 Pinhole Camera 的完整推導
docs/03_intersection.md — 三角形（Möller–Trumbore）、球體、圓錐求交完整數學
docs/04_lighting.md     — Lambert 定律、Phong 高光、衰減、Shadow Ray 物理意義
docs/05_rendering.md    — 渲染管線、超採樣取樣定理、Gamma 2.2 推導、PPM 格式

寫作要求：
- 使用 LaTeX 語法寫數學公式（GitHub Markdown 支援）
- 每個公式都要有對應的 C 代碼片段，讓讀者看到「公式如何變成代碼」
- 包含直覺解說（不只是推導），讓高中數學程度的讀者也能理解
- 表格整理重點對照（公式 / 代碼 / 用途）
- 篇章之間有明確的導引連結

這套文件的目標讀者：懂基礎 C 語言，但沒有學過電腦圖學或線性代數。
```

---

### Prompt 4：加入新幾何體

```
請在現有的 Object tagged union 架構下，新增以下幾何體：
1. 無限平面（Plane）：法線向量 + 平面上一點，解析求交
2. 圓柱體（Cylinder）：含上下蓋圓盤，解析幾何求交

要求：
- 在 scene.h 擴充 GeomType enum 和 union
- 撰寫完整的求交函式，包含法線計算
- 在 main.c 更新場景，展示新幾何體
- 在 docs/03_intersection.md 補充這兩個幾何體的數學推導

請先解釋每種幾何體的隱式方程式和求交推導，再寫代碼。
```

---

### Prompt 5：物理基礎渲染（PBR 入門）

```
Phong 模型是經驗模型，現代遊戲和電影使用 PBR（Physically Based Rendering）。
請幫我：

1. 解釋 Phong 與 PBR 的本質差異（BRDF 的概念）
2. 用以下材質參數替換現有的 Phong Material：
   - metallic（金屬度，0=非金屬，1=金屬）
   - roughness（粗糙度，0=鏡面，1=完全漫射）
   - albedo（基底顏色）
3. 實作簡化版 Cook-Torrance BRDF：
   - D 項：GGX 法線分佈函數
   - F 項：Schlick 近似 Fresnel 方程式
   - G 項：Smith 幾何遮蔽函數
4. 渲染同樣的場景，比較 Phong 和 PBR 的視覺差異

請先推導每個函式的物理意義，再寫代碼。
```

---

### Prompt 6：效能優化

```
目前的渲染器對每條光線遍歷所有物件（O(N) 求交）。
請幫我：

1. 解釋 BVH（Bounding Volume Hierarchy）的原理和建構算法
2. 實作 AABB（Axis-Aligned Bounding Box）包圍盒的快速求交
3. 建構 BVH 樹（使用 SAH：Surface Area Heuristic 分割策略）
4. 量測加速前後的渲染時間（在場景中放置 100 個物件）
5. 解釋為什麼 BVH 能將複雜度從 O(N) 降至 O(log N)

請保持代碼與現有架構相容（Object 介面不變）。
```

---

## 提問模板

在學習過程中，遇到不懂的地方，用這些模板問 AI：

### 理解公式
```
在 [檔案名] 的 [函式名] 中，這行代碼：

    [貼上代碼]

對應到哪個數學公式？請從第一原理推導，
並解釋每個變數的物理意義。
```

### 理解設計決策
```
為什麼 [函式/結構] 要這樣設計？
如果改成 [你的想法]，會有什麼問題？
請從數學正確性、效能、可擴充性三個角度分析。
```

### 視覺問題偵錯
```
我的渲染結果出現 [描述問題，例如：黑色斑點/邊緣鋸齒/顏色不對]。
請幫我分析可能的原因，並說明每個原因在代碼中對應的位置。
```

### 延伸學習
```
我已經理解了 [主題]。
如果我想繼續深入，下一步應該學習什麼？
請給出學習路線，以及推薦的資源（書籍/論文/開源專案）。
```

---

## 學習路線建議

```
【基礎】（本專案涵蓋）
  向量數學 → 光線追蹤原理 → 幾何求交 → Phong 光照 → 渲染管線

  ↓ 掌握基礎後

【進階一：更好看】
  PBR 材質 → 環境貼圖（HDR）→ 景深（Defocus Blur）→ 動態模糊

【進階二：更快】
  BVH 加速 → 多執行緒 → GPU（CUDA/Vulkan Ray Tracing）

【進階三：更真實】
  路徑追蹤（Path Tracing）→ 蒙地卡羅積分 → 重要性取樣
  → 雙向路徑追蹤（BDPT）→ MLT

【學術方向】
  離線渲染：PBRT（書 + 開源實作）
  即時渲染：Real-Time Rendering（第 4 版）
  光譜渲染：Mitsuba 3
```

---

## 配合 AI 學習的最佳實踐

**Do：**
- 每次只問一個問題，等理解了再問下一個
- 要求 AI 「先解釋原理，再給代碼」，不要直接要代碼
- 主動告訴 AI 你卡在哪裡（「我理解到 X，但不懂為什麼 Y」）
- 讓 AI 幫你畫示意圖或公式，視覺化比純文字更容易理解
- 修改代碼後，把結果描述給 AI，讓它幫你分析對不對

**Don't：**
- 不要直接複製代碼，沒有看懂就跑——先讀懂每一行
- 不要一次問太多問題，AI 的回答會流於表面
- 不要跳過數學推導，數學是理解「為什麼這樣寫」的基礎
- 不要只問「怎麼做」，也要問「為什麼這樣做」和「有什麼代價」

---

## 版本資訊

| 項目 | 內容 |
|------|------|
| 語言 | C11 |
| 建置 | CMake 3.15+ |
| 幾何體 | 三角形、球體、圓錐（含底面） |
| 光照 | Phong（環境光 + 漫射 + 高光 + 陰影） |
| 相機 | Pinhole，可控 position / lookat / FOV |
| 抗鋸齒 | Jitter 超採樣 |
| 輸出 | PPM P6，Gamma 2.2 校正 |
| 文件 | 6 篇 Markdown，含 LaTeX 公式與代碼對照 |

---

*這份 Golden Prompt 由 Claude（Anthropic）協助設計。*  
*歡迎 fork、修改、分享——學習本來就應該是開放的。*
