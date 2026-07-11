# Motion SDK C++ examples
# 由仓库根 CMakeLists.txt 触发；
# 依赖父级提供的变量：UNIUBI_SDK_INCLUDE_DIR、UNIUBI_ROBOT_MOTION_SDK_LIB

include_directories(${UNIUBI_SDK_INCLUDE_DIR})

add_executable(example_lowlevel example_lowlevel.cpp)
target_link_libraries(example_lowlevel ${UNIUBI_ROBOT_MOTION_SDK_LIB} pthread)

add_executable(example_highlevel example_highlevel.cpp)
target_link_libraries(example_highlevel ${UNIUBI_ROBOT_MOTION_SDK_LIB} pthread)

set(UNIUBI_EXAMPLE_TARGETS example_lowlevel example_highlevel)

if(BUILD_SDK_MEDIA_EXAMPLE)
    add_executable(example_media_frames example_media_frames.cpp)
    target_link_libraries(example_media_frames
        ${UNIUBI_ROBOT_MOTION_SDK_LIB}
        ${UNIUBI_MEDIABUS_LIB}
        ${UNIUBI_UBASE_LIB}
        pthread)
    list(APPEND UNIUBI_EXAMPLE_TARGETS example_media_frames)
endif()

install(TARGETS ${UNIUBI_EXAMPLE_TARGETS} RUNTIME DESTINATION bin)
