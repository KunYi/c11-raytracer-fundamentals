cmake_minimum_required(VERSION 3.15)
project(raytracer LANGUAGES C)

# ── 強制 C11 ──
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

# ── 預設 Release 建置 ──
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ── 尋找 zlib（PNG 壓縮所需，幾乎所有平台均內建）──
find_package(ZLIB REQUIRED)

# ── 來源檔案 ──
set(SOURCES
    src/main.c
    src/renderer.c
)

add_executable(raytracer ${SOURCES})

# ── Include 路徑 ──
target_include_directories(raytracer PRIVATE include)

# ── 編譯選項 ──
target_compile_options(raytracer PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:
        -Wall
        -Wextra
        -Wpedantic
        -O3
        -ffast-math
        -march=native
    >
    $<$<C_COMPILER_ID:MSVC>:
        /W4
        /O2
    >
)

# ── OpenMP（選用，找不到時仍可正常建置，只是不啟用並行）──
find_package(OpenMP)
if(OpenMP_C_FOUND)
    message(STATUS "OpenMP    : 已啟用（${OpenMP_C_VERSION}）")
    target_link_libraries(raytracer PRIVATE OpenMP::OpenMP_C)
    target_compile_definitions(raytracer PRIVATE _OPENMP_ENABLED)
else()
    message(STATUS "OpenMP    : 未找到，使用單執行緒模式")
endif()

# ── 連結：數學庫（僅 GNU/Clang 需要 -lm，MSVC 內建）+ zlib ──
target_link_libraries(raytracer PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:m>
    ZLIB::ZLIB
)

# ── 安裝（可選） ──
install(TARGETS raytracer DESTINATION bin)

message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "C standard : C${CMAKE_C_STANDARD}")
message(STATUS "zlib version: ${ZLIB_VERSION_STRING}")

# ══════════════════════════════════════
#  測試目標（CTest）
#  建置：cmake .. && make
#  執行：ctest --output-on-failure
#      或各執行檔單獨執行
# ══════════════════════════════════════
enable_testing()

set(RENDERER_SRC src/renderer.c)

function(add_raytracer_test name src)
    add_executable(${name} ${src} ${RENDERER_SRC})
    target_include_directories(${name} PRIVATE include tests)
    target_compile_options(${name} PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>: -Wall -Wextra -O0 -g >
        $<$<C_COMPILER_ID:MSVC>: /W4 >
    )
    target_link_libraries(${name} PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:m>
        ZLIB::ZLIB
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_raytracer_test(test_vec3         tests/test_vec3.c)
add_raytracer_test(test_ray          tests/test_ray.c)
add_raytracer_test(test_intersection tests/test_intersection.c)
add_raytracer_test(test_renderer     tests/test_renderer.c)
