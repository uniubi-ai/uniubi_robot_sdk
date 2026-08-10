# Motion SDK C++ examples
# 由仓库根 CMakeLists.txt 触发；
# 依赖父级提供的变量：UNIUBI_SDK_INCLUDE_DIR、UNIUBI_ROBOT_MOTION_SDK_LIB

include_directories(${UNIUBI_SDK_INCLUDE_DIR})

add_executable(example_lowlevel example_lowlevel.cpp)
target_link_libraries(example_lowlevel ${UNIUBI_ROBOT_MOTION_SDK_LIB} pthread)

add_executable(example_highlevel example_highlevel.cpp)
target_link_libraries(example_highlevel ${UNIUBI_ROBOT_MOTION_SDK_LIB} pthread)

add_executable(example_odometry example_odometry.cpp)
target_link_libraries(example_odometry ${UNIUBI_ROBOT_MOTION_SDK_LIB} pthread)

set(UNIUBI_EXAMPLE_TARGETS example_lowlevel example_highlevel example_odometry)

if(BUILD_SDK_MEDIA_EXAMPLE)
    add_executable(example_media_frames example_media_frames.cpp)
    target_link_libraries(example_media_frames
        ${UNIUBI_ROBOT_MOTION_SDK_LIB}
        ${UNIUBI_MEDIABUS_LIB}
        ${UNIUBI_UBASE_LIB}
        pthread)
    list(APPEND UNIUBI_EXAMPLE_TARGETS example_media_frames)
endif()

# The prebuilt aarch64 SDK depends on libraries that are resolved on the Orin
# target (CUDA/Jetson runtime) and on sibling libraries in the SDK package.
# During cross-linking, search the selected SDK library directory for DT_NEEDED
# dependencies and leave target-only shared-library symbols for runtime loading.
if(CMAKE_CROSSCOMPILING AND UNIUBI_SDK_ARCH_DIR STREQUAL "aarch64")
    get_filename_component(UNIUBI_SDK_LIBRARY_DIR
        "${UNIUBI_ROBOT_MOTION_SDK_LIB}" DIRECTORY)
    foreach(example_target IN LISTS UNIUBI_EXAMPLE_TARGETS)
        target_link_options(${example_target} PRIVATE
            "-Wl,-rpath-link,${UNIUBI_SDK_LIBRARY_DIR}"
            "-Wl,--allow-shlib-undefined")
    endforeach()
endif()

install(TARGETS ${UNIUBI_EXAMPLE_TARGETS} RUNTIME DESTINATION bin)
