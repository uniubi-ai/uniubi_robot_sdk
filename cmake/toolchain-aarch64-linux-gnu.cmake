# Cross toolchain：x86_64 主机 → aarch64-linux-gnu 目标
# 用法：cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake ...
#
# 依赖（Debian/Ubuntu）：
#   sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
#
# 如目标系统有 sysroot，把 CMAKE_SYSROOT 指向它，再放开 find_root_path_*

set(CMAKE_SYSTEM_NAME      Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(CMAKE_C_COMPILER   aarch64-linux-gnu-gcc)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# 可选：指向 aarch64 sysroot（含 /usr/lib /usr/include 等）
# set(CMAKE_SYSROOT /opt/aarch64-sysroot)
# list(APPEND CMAKE_FIND_ROOT_PATH ${CMAKE_SYSROOT})

# 主机工具（如 pkg-config）仍走宿主；库 / 头 / find_package 走目标 sysroot
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
