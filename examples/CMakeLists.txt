# Motion SDK C++ examples
#
# 支持两种入口：
#   1. SDK 源码树：cmake -S . -B build
#   2. 已安装 SDK：cmake -S examples -B build-examples -DCMAKE_PREFIX_PATH=<prefix>

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.18)
    project(uniubi_robot_sdk_examples LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    find_package(UniubiRobotSdk CONFIG REQUIRED)

    if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
        set(_UNIUBI_MEDIA_EXAMPLE_DEFAULT ON)
    else()
        set(_UNIUBI_MEDIA_EXAMPLE_DEFAULT OFF)
    endif()
    option(BUILD_SDK_MEDIA_EXAMPLE
        "Build MediaBus example; supported only on aarch64 local deployment"
        ${_UNIUBI_MEDIA_EXAMPLE_DEFAULT})
endif()

# TensorRT Low-level 示例默认只在 Orin 原生构建时启用。交叉编译需要目标
# aarch64 的 TensorRT/CUDA 开发文件，因此必须显式打开并提供对应路径。
set(_UNIUBI_TENSORRT_EXAMPLE_DEFAULT OFF)
if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$" AND
   NOT CMAKE_CROSSCOMPILING AND
   (EXISTS "/usr/include/aarch64-linux-gnu/NvInfer.h" OR
    EXISTS "/usr/include/NvInfer.h") AND
   EXISTS "/usr/local/cuda/include/cuda_runtime_api.h")
    set(_UNIUBI_TENSORRT_EXAMPLE_DEFAULT ON)
endif()
option(BUILD_SDK_TENSORRT_EXAMPLE
    "Build the Jetson Orin Low-level TensorRT example"
    ${_UNIUBI_TENSORRT_EXAMPLE_DEFAULT})
set(UNIUBI_TENSORRT_ROOT "" CACHE PATH
    "TensorRT target development root (include and lib/lib64), mainly for cross-compiling")
set(UNIUBI_CUDA_ROOT "" CACHE PATH
    "CUDA target toolkit root (include and lib/lib64), mainly for cross-compiling")

add_executable(example_lowlevel example_lowlevel.cpp)
target_link_libraries(example_lowlevel PRIVATE Uniubi::RobotMotionSdk)

add_executable(example_highlevel example_highlevel.cpp)
target_link_libraries(example_highlevel PRIVATE Uniubi::RobotMotionSdk)

set(UNIUBI_EXAMPLE_TARGETS example_lowlevel example_highlevel)

if(BUILD_SDK_TENSORRT_EXAMPLE)
    set(_UNIUBI_NVIDIA_FIND_OPTIONS)
    if(CMAKE_CROSSCOMPILING)
        if(NOT UNIUBI_TENSORRT_ROOT OR NOT UNIUBI_CUDA_ROOT)
            message(FATAL_ERROR
                "Cross-compiling example_lowlevel_tensorrt requires both "
                "UNIUBI_TENSORRT_ROOT and UNIUBI_CUDA_ROOT for the target aarch64 platform.")
        endif()
        list(APPEND _UNIUBI_NVIDIA_FIND_OPTIONS
            NO_DEFAULT_PATH NO_CMAKE_FIND_ROOT_PATH)
    endif()

    find_path(UNIUBI_TENSORRT_INCLUDE_DIR
        NAMES NvInfer.h
        HINTS ${UNIUBI_TENSORRT_ROOT}/include
              ${UNIUBI_TENSORRT_ROOT}/include/aarch64-linux-gnu
        ${_UNIUBI_NVIDIA_FIND_OPTIONS})
    find_path(UNIUBI_TENSORRT_ONNX_INCLUDE_DIR
        NAMES NvOnnxParser.h
        HINTS ${UNIUBI_TENSORRT_ROOT}/include
              ${UNIUBI_TENSORRT_ROOT}/include/aarch64-linux-gnu
        ${_UNIUBI_NVIDIA_FIND_OPTIONS})
    find_library(UNIUBI_NVINFER_LIBRARY
        NAMES nvinfer
        HINTS ${UNIUBI_TENSORRT_ROOT}/lib
              ${UNIUBI_TENSORRT_ROOT}/lib64
              ${UNIUBI_TENSORRT_ROOT}/lib/aarch64-linux-gnu
        ${_UNIUBI_NVIDIA_FIND_OPTIONS})
    find_library(UNIUBI_NVONNXPARSER_LIBRARY
        NAMES nvonnxparser
        HINTS ${UNIUBI_TENSORRT_ROOT}/lib
              ${UNIUBI_TENSORRT_ROOT}/lib64
              ${UNIUBI_TENSORRT_ROOT}/lib/aarch64-linux-gnu
        ${_UNIUBI_NVIDIA_FIND_OPTIONS})
    find_path(UNIUBI_CUDA_INCLUDE_DIR
        NAMES cuda_runtime_api.h
        HINTS ${UNIUBI_CUDA_ROOT}/include /usr/local/cuda/include
        ${_UNIUBI_NVIDIA_FIND_OPTIONS})
    find_library(UNIUBI_CUDART_LIBRARY
        NAMES cudart
        HINTS ${UNIUBI_CUDA_ROOT}/lib
              ${UNIUBI_CUDA_ROOT}/lib64
              ${UNIUBI_CUDA_ROOT}/targets/aarch64-linux/lib
              /usr/local/cuda/lib64
        ${_UNIUBI_NVIDIA_FIND_OPTIONS})

    foreach(required_var
            UNIUBI_TENSORRT_INCLUDE_DIR
            UNIUBI_TENSORRT_ONNX_INCLUDE_DIR
            UNIUBI_NVINFER_LIBRARY
            UNIUBI_NVONNXPARSER_LIBRARY
            UNIUBI_CUDA_INCLUDE_DIR
            UNIUBI_CUDART_LIBRARY)
        if(NOT ${required_var})
            message(FATAL_ERROR
                "BUILD_SDK_TENSORRT_EXAMPLE=ON but ${required_var} was not found. "
                "Build natively on JetPack 6.2.1, or provide matching aarch64 "
                "UNIUBI_TENSORRT_ROOT and UNIUBI_CUDA_ROOT paths.")
        endif()
    endforeach()

    add_executable(example_lowlevel_tensorrt example_lowlevel_tensorrt.cpp)
    target_include_directories(example_lowlevel_tensorrt PRIVATE
        ${UNIUBI_TENSORRT_INCLUDE_DIR}
        ${UNIUBI_TENSORRT_ONNX_INCLUDE_DIR}
        ${UNIUBI_CUDA_INCLUDE_DIR})
    target_link_libraries(example_lowlevel_tensorrt PRIVATE
        Uniubi::RobotMotionSdk
        ${UNIUBI_NVINFER_LIBRARY}
        ${UNIUBI_NVONNXPARSER_LIBRARY}
        ${UNIUBI_CUDART_LIBRARY})
    list(APPEND UNIUBI_EXAMPLE_TARGETS example_lowlevel_tensorrt)
endif()

if(BUILD_SDK_MEDIA_EXAMPLE)
    if(NOT TARGET Uniubi::MediaBus)
        message(FATAL_ERROR "BUILD_SDK_MEDIA_EXAMPLE=ON but Uniubi::MediaBus is unavailable")
    endif()
    add_executable(example_media_frames example_media_frames.cpp)
    target_link_libraries(example_media_frames
        PRIVATE Uniubi::RobotMotionSdk Uniubi::MediaBus)
    list(APPEND UNIUBI_EXAMPLE_TARGETS example_media_frames)
endif()

# The prebuilt aarch64 SDK depends on libraries that are resolved on the Orin
# target (CUDA/Jetson runtime) and on sibling libraries in the SDK package.
# During cross-linking, search the selected SDK library directory for DT_NEEDED
# dependencies and leave target-only shared-library symbols for runtime loading.
if(CMAKE_CROSSCOMPILING AND UNIUBI_SDK_ARCH_DIR STREQUAL "aarch64")
    get_target_property(UNIUBI_SDK_RUNTIME_LIBRARY
        Uniubi::RobotMotionSdk IMPORTED_LOCATION)
    get_filename_component(UNIUBI_SDK_LIBRARY_DIR
        "${UNIUBI_SDK_RUNTIME_LIBRARY}" DIRECTORY)
    foreach(example_target IN LISTS UNIUBI_EXAMPLE_TARGETS)
        target_link_options(${example_target} PRIVATE
            "-Wl,-rpath-link,${UNIUBI_SDK_LIBRARY_DIR}"
            "-Wl,--allow-shlib-undefined")
    endforeach()
endif()

install(TARGETS ${UNIUBI_EXAMPLE_TARGETS} RUNTIME DESTINATION bin)
