cmake_minimum_required(VERSION 3.10)
project(huashan_bridge C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# ── 编译选项 ──
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Os -ffunction-sections -fdata-sections")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--gc-sections -static -lm")

# ── 源文件 ──
set(SOURCES
    src/main.c
    src/sliding_window.c
    src/feature_extract.c
    src/fault_detect.c
)

add_executable(huashan_bridge ${SOURCES})

# ── 安装到 /mnt/data/ (华山派数据分区) ──
install(TARGETS huashan_bridge
        RUNTIME DESTINATION /mnt/data)
