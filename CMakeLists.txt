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

# ── 連結數學庫（Linux 需要 -lm） ──
target_link_libraries(raytracer PRIVATE m ZLIB::ZLIB)

# ── 安裝（可選） ──
install(TARGETS raytracer DESTINATION bin)

message(STATUS "Build type : ${CMAKE_BUILD_TYPE}")
message(STATUS "C standard : C${CMAKE_C_STANDARD}")
