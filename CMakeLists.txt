# Uniubi Robot Motion SDK —— C++ SDK 工程入口
# 编译公开面：C++ examples
#
# ───────────────────────────────────────────────────────────────
# 用法 1：本地构建（默认走系统编译器，目标架构 = 本机）
# ───────────────────────────────────────────────────────────────
#   cd uniubi_robot_sdk
#   cmake -S . -B build [-DUNIUBI_SDK_ROOT=<闭源 SDK 安装前缀>]
#   cmake --build build -j
#
# ───────────────────────────────────────────────────────────────
# 用法 2：交叉编译（指定工具链文件）
# ───────────────────────────────────────────────────────────────
# 通过 CMake 标准变量 CMAKE_TOOLCHAIN_FILE 切换编译器与目标架构。
# 仓库自带样例工具链：cmake/toolchain-aarch64-linux-gnu.cmake
#
# 示例（在 x86_64 主机交叉编 aarch64 板上运行的产物）：
#
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#   cmake -S . -B build-aarch64 \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake \
#         [-DUNIUBI_SDK_ROOT=/path/to/sdk]      # toolchain 设 CMAKE_SYSTEM_PROCESSOR=aarch64，自动取 lib/aarch64/
#   cmake --build build-aarch64 -j
#
# 自定义工具链：拷贝 cmake/toolchain-*.cmake 改 CMAKE_C/CXX_COMPILER / SYSROOT 即可。
#
# ───────────────────────────────────────────────────────────────
# 闭源 SDK 共享库 robotMotionSdk 的查找顺序（仅查 lib/<arch>/）：
# ───────────────────────────────────────────────────────────────
#   1. ${UNIUBI_SDK_ROOT}/lib/<arch>                （cmake -D 或环境变量）
#   2. ${CMAKE_CURRENT_SOURCE_DIR}/lib/<arch>       （仓库内自带）
#   3. /opt/uniubi/lib/<arch>                        （默认前缀）
#
# <arch> 由 CMAKE_SYSTEM_PROCESSOR 自动决定：x86_64 / aarch64 / i386
# 交叉编译时由工具链文件设置 CMAKE_SYSTEM_PROCESSOR，无需手动指定目标 arch
#
# 关闭 C++ examples：cmake -DBUILD_SDK_CPP_EXAMPLES=OFF

cmake_minimum_required(VERSION 3.18)

project(uniubi_robot_motion_sdk
    VERSION 0.1.0
    DESCRIPTION "Uniubi Robot Motion SDK – C++ examples"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 闭源 SDK 根目录（含 lib/<arch>/ 和 include/）
if(NOT UNIUBI_SDK_ROOT AND DEFINED ENV{UNIUBI_SDK_ROOT})
    set(UNIUBI_SDK_ROOT $ENV{UNIUBI_SDK_ROOT})
endif()

# 按目标架构 (CMAKE_SYSTEM_PROCESSOR) 选择 Lib 子目录
# 支持：x86_64 / aarch64 / i386；交叉编译时由工具链文件设置 CMAKE_SYSTEM_PROCESSOR
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(UNIUBI_SDK_ARCH_DIR "x86_64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    set(UNIUBI_SDK_ARCH_DIR "aarch64")
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86)$")
    set(UNIUBI_SDK_ARCH_DIR "i386")
else()
    message(FATAL_ERROR "Unsupported CMAKE_SYSTEM_PROCESSOR='${CMAKE_SYSTEM_PROCESSOR}'; expected x86_64 / aarch64 / i386")
endif()

find_library(UNIUBI_ROBOT_MOTION_SDK_LIB
    NAMES robotMotionSdk
    HINTS ${UNIUBI_SDK_ROOT}/lib/${UNIUBI_SDK_ARCH_DIR}
          ${CMAKE_CURRENT_SOURCE_DIR}/lib/${UNIUBI_SDK_ARCH_DIR}
          /opt/uniubi/lib/${UNIUBI_SDK_ARCH_DIR}
    NO_DEFAULT_PATH
    NO_CMAKE_FIND_ROOT_PATH      # 闭源 SDK 不在 sysroot 里，跨编不要被 toolchain 重定向
    REQUIRED)

# 当前 librobotMotionSdk.so 对 mediaBus/ubase 有动态依赖，运行库包必须同版本同架构完整提供。
if(NOT UNIUBI_MEDIABUS_LIB)
    find_library(UNIUBI_MEDIABUS_LIB
        NAMES mediaBus
        HINTS ${UNIUBI_SDK_ROOT}/lib/${UNIUBI_SDK_ARCH_DIR}
              ${CMAKE_CURRENT_SOURCE_DIR}/lib/${UNIUBI_SDK_ARCH_DIR}
              /opt/uniubi/lib/${UNIUBI_SDK_ARCH_DIR}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        REQUIRED)
endif()

if(NOT UNIUBI_UBASE_LIB)
    find_library(UNIUBI_UBASE_LIB
        NAMES ubase
        HINTS ${UNIUBI_SDK_ROOT}/lib/${UNIUBI_SDK_ARCH_DIR}
              ${CMAKE_CURRENT_SOURCE_DIR}/lib/${UNIUBI_SDK_ARCH_DIR}
              /opt/uniubi/lib/${UNIUBI_SDK_ARCH_DIR}
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
        REQUIRED)
endif()

set(UNIUBI_MEDIABUS_SUPPORTED OFF)
if(UNIUBI_SDK_ARCH_DIR STREQUAL "aarch64")
    set(UNIUBI_MEDIABUS_SUPPORTED ON)
endif()

set(BUILD_SDK_MEDIA_EXAMPLE_DEFAULT ${UNIUBI_MEDIABUS_SUPPORTED})
option(BUILD_SDK_MEDIA_EXAMPLE "Build MediaBus C++ example; supported only on aarch64 local deployment" ${BUILD_SDK_MEDIA_EXAMPLE_DEFAULT})

if(BUILD_SDK_MEDIA_EXAMPLE AND NOT UNIUBI_MEDIABUS_SUPPORTED)
    message(WARNING "MediaBus example is only supported on aarch64 local deployment; disabling for ${UNIUBI_SDK_ARCH_DIR}.")
    set(BUILD_SDK_MEDIA_EXAMPLE OFF CACHE BOOL "Build MediaBus C++ example; supported only on aarch64 local deployment" FORCE)
endif()

# 公开头根：随仓库自带（include/），无需外部供给
set(UNIUBI_SDK_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include CACHE PATH "Uniubi SDK public headers root")

message(STATUS "Uniubi SDK arch:    ${UNIUBI_SDK_ARCH_DIR} (processor=${CMAKE_SYSTEM_PROCESSOR})")
message(STATUS "Uniubi SDK library: ${UNIUBI_ROBOT_MOTION_SDK_LIB}")
message(STATUS "Uniubi MediaBus library: ${UNIUBI_MEDIABUS_LIB} (runtime dependency; client API supported only on aarch64 local deployment)")
message(STATUS "Uniubi UBase library: ${UNIUBI_UBASE_LIB}")
if(NOT BUILD_SDK_MEDIA_EXAMPLE)
    message(STATUS "Uniubi MediaBus example: disabled (${UNIUBI_SDK_ARCH_DIR}; supported only on aarch64 local deployment)")
endif()
message(STATUS "Uniubi SDK include: ${UNIUBI_SDK_INCLUDE_DIR}")

option(BUILD_SDK_CPP_EXAMPLES "Build C++ SDK examples" ON)

if(BUILD_SDK_CPP_EXAMPLES)
    add_subdirectory(examples)
endif()
