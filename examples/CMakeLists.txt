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

add_executable(example_lowlevel example_lowlevel.cpp)
target_link_libraries(example_lowlevel PRIVATE Uniubi::RobotMotionSdk)

add_executable(example_highlevel example_highlevel.cpp)
target_link_libraries(example_highlevel PRIVATE Uniubi::RobotMotionSdk)

set(UNIUBI_EXAMPLE_TARGETS example_lowlevel example_highlevel)

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
