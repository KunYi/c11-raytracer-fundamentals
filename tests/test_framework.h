#ifndef TEST_FRAMEWORK_H
#define TEST_FRAMEWORK_H

/*
 * test_framework.h — 極簡 TDD 框架（純 C11 標準，跨平台）
 *
 * 設計原則：
 *   - 零編譯器擴充：不使用 __attribute__((constructor))
 *     改為在每個測試檔的 main() 中明確呼叫 RUN_SUITE()
 *   - 支援 GCC / Clang / MSVC
 *
 * 用法：
 *   TEST(suite_name, test_name) { ... }   定義一個測試函式
 *   ASSERT_TRUE(expr)
 *   ASSERT_FALSE(expr)
 *   ASSERT_EQ(a, b)
 *   ASSERT_NEAR(a, b, eps)
 *   ASSERT_VEC3_NEAR(v, x, y, z, eps)
 *
 *   在 main() 裡：
 *     RUN_SUITE(suite_name, test1, test2, ...);  執行該 suite 的所有測試
 *     return REPORT();                           輸出摘要並回傳結果
 *
 * 範例：
 *   TEST(math, add) { ASSERT_EQ(1+1, 2); }
 *   TEST(math, sub) { ASSERT_EQ(3-1, 2); }
 *
 *   int main(void) {
 *       RUN_SUITE(math, add, sub);
 *       return REPORT();
 *   }
 */

#include <stdio.h>
#include <math.h>

/* ── 全域統計（每個 .c 各自獨立） ── */
static int _tests_run    = 0;
static int _tests_passed = 0;
static int _tests_failed = 0;

static const char *_cur_suite  = "";
static const char *_cur_test   = "";
static int         _cur_failed = 0;

/* ════════════════════════════════════════
 *  ASSERT 巨集
 * ════════════════════════════════════════ */

#define ASSERT_TRUE(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "  [FAIL] %s::%s — line %d: ASSERT_TRUE(%s)\n", \
                _cur_suite, _cur_test, __LINE__, #expr); \
        if (!_cur_failed) { _tests_failed++; _cur_failed = 1; } \
        return; \
    } \
} while(0)

#define ASSERT_FALSE(expr) do { \
    if ((expr)) { \
        fprintf(stderr, "  [FAIL] %s::%s — line %d: ASSERT_FALSE(%s)\n", \
                _cur_suite, _cur_test, __LINE__, #expr); \
        if (!_cur_failed) { _tests_failed++; _cur_failed = 1; } \
        return; \
    } \
} while(0)

#define ASSERT_EQ(a, b) do { \
    if ((a) != (b)) { \
        fprintf(stderr, "  [FAIL] %s::%s — line %d: ASSERT_EQ(%s, %s) " \
                "got (%lld != %lld)\n", \
                _cur_suite, _cur_test, __LINE__, #a, #b, \
                (long long)(a), (long long)(b)); \
        if (!_cur_failed) { _tests_failed++; _cur_failed = 1; } \
        return; \
    } \
} while(0)

#define ASSERT_NEAR(a, b, eps) do { \
    double _a=(double)(a), _b=(double)(b), _e=(double)(eps); \
    if (fabs(_a-_b) > _e) { \
        fprintf(stderr, "  [FAIL] %s::%s — line %d: ASSERT_NEAR(%s, %s, %s) " \
                "got (%.10g vs %.10g, diff=%.3e > eps=%.3e)\n", \
                _cur_suite, _cur_test, __LINE__, #a, #b, #eps, \
                _a, _b, fabs(_a-_b), _e); \
        if (!_cur_failed) { _tests_failed++; _cur_failed = 1; } \
        return; \
    } \
} while(0)

#define ASSERT_VEC3_NEAR(v, ex, ey, ez, eps) do { \
    ASSERT_NEAR((v).x, ex, eps); \
    ASSERT_NEAR((v).y, ey, eps); \
    ASSERT_NEAR((v).z, ez, eps); \
} while(0)

/* ════════════════════════════════════════
 *  TEST 巨集：只宣告函式，不做任何自動登錄
 * ════════════════════════════════════════ */
#define TEST(suite, name) \
    static void _test_##suite##_##name(void)

/* ════════════════════════════════════════
 *  _RUN_ONE：執行單個測試並更新統計
 * ════════════════════════════════════════ */
#define _RUN_ONE(suite, name) do { \
    _cur_suite  = #suite; \
    _cur_test   = #name; \
    _cur_failed = 0; \
    _tests_run++; \
    _test_##suite##_##name(); \
    if (!_cur_failed) { \
        fprintf(stderr, "  [PASS] %s::%s\n", #suite, #name); \
        _tests_passed++; \
    } \
} while(0)

/* ════════════════════════════════════════
 *  RUN_SUITE：執行一個 suite 內的所有測試
 *
 *  用法：RUN_SUITE(suite, test1, test2, test3, ...);
 *
 *  利用 __VA_ARGS__ 展開，最多支援 32 個測試（可按需擴充）。
 *  標準 C11 的 __VA_ARGS__ 不支援計數，
 *  改用 _FOR_EACH 展開至 _RUN_ONE 呼叫。
 * ════════════════════════════════════════ */

/* _FOR_EACH：對每個參數展開 ACTION(suite, x) */
#define _FE_1( A,S,x)            _RUN_ONE(S,x)
#define _FE_2( A,S,x,...)  A(A,S,x); _FE_1( A,S,__VA_ARGS__)
#define _FE_3( A,S,x,...)  A(A,S,x); _FE_2( A,S,__VA_ARGS__)
#define _FE_4( A,S,x,...)  A(A,S,x); _FE_3( A,S,__VA_ARGS__)
#define _FE_5( A,S,x,...)  A(A,S,x); _FE_4( A,S,__VA_ARGS__)
#define _FE_6( A,S,x,...)  A(A,S,x); _FE_5( A,S,__VA_ARGS__)
#define _FE_7( A,S,x,...)  A(A,S,x); _FE_6( A,S,__VA_ARGS__)
#define _FE_8( A,S,x,...)  A(A,S,x); _FE_7( A,S,__VA_ARGS__)
#define _FE_9( A,S,x,...)  A(A,S,x); _FE_8( A,S,__VA_ARGS__)
#define _FE_10(A,S,x,...)  A(A,S,x); _FE_9( A,S,__VA_ARGS__)
#define _FE_11(A,S,x,...)  A(A,S,x); _FE_10(A,S,__VA_ARGS__)
#define _FE_12(A,S,x,...)  A(A,S,x); _FE_11(A,S,__VA_ARGS__)
#define _FE_13(A,S,x,...)  A(A,S,x); _FE_12(A,S,__VA_ARGS__)
#define _FE_14(A,S,x,...)  A(A,S,x); _FE_13(A,S,__VA_ARGS__)
#define _FE_15(A,S,x,...)  A(A,S,x); _FE_14(A,S,__VA_ARGS__)
#define _FE_16(A,S,x,...)  A(A,S,x); _FE_15(A,S,__VA_ARGS__)
#define _FE_17(A,S,x,...)  A(A,S,x); _FE_16(A,S,__VA_ARGS__)
#define _FE_18(A,S,x,...)  A(A,S,x); _FE_17(A,S,__VA_ARGS__)
#define _FE_19(A,S,x,...)  A(A,S,x); _FE_18(A,S,__VA_ARGS__)
#define _FE_20(A,S,x,...)  A(A,S,x); _FE_19(A,S,__VA_ARGS__)
#define _FE_21(A,S,x,...)  A(A,S,x); _FE_20(A,S,__VA_ARGS__)
#define _FE_22(A,S,x,...)  A(A,S,x); _FE_21(A,S,__VA_ARGS__)
#define _FE_23(A,S,x,...)  A(A,S,x); _FE_22(A,S,__VA_ARGS__)
#define _FE_24(A,S,x,...)  A(A,S,x); _FE_23(A,S,__VA_ARGS__)
#define _FE_25(A,S,x,...)  A(A,S,x); _FE_24(A,S,__VA_ARGS__)
#define _FE_26(A,S,x,...)  A(A,S,x); _FE_25(A,S,__VA_ARGS__)
#define _FE_27(A,S,x,...)  A(A,S,x); _FE_26(A,S,__VA_ARGS__)
#define _FE_28(A,S,x,...)  A(A,S,x); _FE_27(A,S,__VA_ARGS__)
#define _FE_29(A,S,x,...)  A(A,S,x); _FE_28(A,S,__VA_ARGS__)
#define _FE_30(A,S,x,...)  A(A,S,x); _FE_29(A,S,__VA_ARGS__)
#define _FE_31(A,S,x,...)  A(A,S,x); _FE_30(A,S,__VA_ARGS__)
#define _FE_32(A,S,x,...)  A(A,S,x); _FE_31(A,S,__VA_ARGS__)

/* 根據參數個數選擇對應的 _FE_N */
#define _FE_N(_1,_2,_3,_4,_5,_6,_7,_8,_9,_10, \
              _11,_12,_13,_14,_15,_16,_17,_18,_19,_20, \
              _21,_22,_23,_24,_25,_26,_27,_28,_29,_30, \
              _31,_32, N, ...) _FE_##N

#define _FOR_EACH(A, S, ...) \
    _FE_N(__VA_ARGS__, \
          32,31,30,29,28,27,26,25,24,23,22,21,20, \
          19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1) \
    (A, S, __VA_ARGS__)

#define _EXPAND_RUN(A, S, x) _RUN_ONE(S, x)

#define RUN_SUITE(suite, ...) do { \
    fprintf(stderr, "\n[Suite: %s]\n", #suite); \
    _FOR_EACH(_EXPAND_RUN, suite, __VA_ARGS__); \
} while(0)

/* ════════════════════════════════════════
 *  REPORT：輸出摘要，回傳 exit code
 * ════════════════════════════════════════ */
#define REPORT() ( \
    fprintf(stderr, \
        "\n========================================\n" \
        " 共 %d 個測試：%d 通過，%d 失敗\n" \
        "========================================\n\n", \
        _tests_run, _tests_passed, _tests_failed), \
    (_tests_failed > 0) ? 1 : 0 \
)

#endif /* TEST_FRAMEWORK_H */
