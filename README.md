# Uniubi Robot SDK

宇泛智能机器狗 C++ 运动控制 SDK，提供高级控制、低级控制和媒体总线能力。本仓库保留源码、头文件、预编译运行库和 C++ examples；完整接口说明统一维护在 [`uniubi-docs`](https://github.com/uniubi-ai/uniubi-docs)。

## 目录结构

```
.
├── CMakeLists.txt
├── cmake/
│   └── toolchain-aarch64-linux-gnu.cmake
├── include/
│   └── uniubi/
│       └── robot_sdk/
│           ├── MotionSdkService.h
│           ├── MotionSdkProtocol.h
│           ├── MotionHighLevelClient.h
│           ├── MotionLowLevelClient.h
│           ├── MediaBusClient.h
│           ├── Media/
│           ├── Memory/
│           └── UBase/
├── lib/
│   ├── x86_64/
│   ├── aarch64/
│   └── i386/
├── examples/
│   ├── example_highlevel.cpp
│   ├── example_lowlevel.cpp
│   └── example_media_frames.cpp
```

## 快速开始

```bash
git clone https://github.com/uniubi-ai/uniubi_robot_sdk.git
cd uniubi_robot_sdk
cmake -S . -B build
cmake --build build -j
```

运行示例前设置动态库路径：

```bash
export LD_LIBRARY_PATH=$PWD/lib/$(uname -m):$LD_LIBRARY_PATH
./build/examples/example_highlevel
./build/examples/example_lowlevel
```

`example_lowlevel` 会进入低级控制模式，并以 50 Hz 连续发送 60 秒零目标、零增益、零前馈力矩控制帧。该流程用于验证通信和观测闭环，不是平衡站立控制器；仅应在机器人上吊架、急停可触达且场地空旷时运行。

媒体帧订阅 demo `example_media_frames` 仅在 `aarch64` 板内本地部署构建和运行；`x86_64` / `i386` 平台不要调用 `createMediaBusClient()`、`setup()` 或 `start*Frame()` 等 media client 接口。运行库包仍需保持同版本、同架构 `.so` 文件成组放置；库查找路径和平台矩阵见 [`uniubi-docs/docs/BUILD.md`](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.md)。

## 集成到 CMake 项目

```cmake
set(UNIUBI_SDK_ROOT "$ENV{HOME}/uniubi_robot_sdk" CACHE PATH "Uniubi SDK root")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  set(ARCH_DIR x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64)$")
  set(ARCH_DIR aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i386|i686|x86)$")
  set(ARCH_DIR i386)
else()
  message(FATAL_ERROR "Unsupported arch: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

find_library(UNIUBI_MOTION_SDK robotMotionSdk
  PATHS ${UNIUBI_SDK_ROOT}/lib/${ARCH_DIR}
  NO_DEFAULT_PATH REQUIRED)

target_include_directories(my_robot_app PRIVATE
  ${UNIUBI_SDK_ROOT}/include)
target_link_libraries(my_robot_app PRIVATE ${UNIUBI_MOTION_SDK} pthread)
```

## 最小 C++ 示例

```cpp
#include <chrono>
#include <thread>
#include "uniubi/robot_sdk/MotionSdkService.h"
#include "uniubi/robot_sdk/MotionHighLevelClient.h"

using namespace uniubi::RobotSdk;

int main() {
    auto service = IMotionSdkService::instance();
    if (!service->initialService(nullptr, "myApp")) {
        return 1;
    }

    auto client = IMotionHighLevelClient::create();
    if (!client || !client->connect() || !client->startControl(30000)) {
        service->shutdown();
        return 1;
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (client->getState() != IMotionHighLevelClient::kControlled) {
        if (std::chrono::steady_clock::now() >= deadline) {
            client->disconnect();
            service->shutdown();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    client->standUp();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    client->lieDown();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    client->releaseControl();
    client->disconnect();
    service->shutdown();
    return 0;
}
```

## 文档

- 本仓运行注意事项：[`docs/runtime_notes.md`](docs/runtime_notes.md)
- 文档总站：[`uniubi-docs`](https://github.com/uniubi-ai/uniubi-docs)
- 构建与部署：[`docs/BUILD.md`](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.md)
- 高级控制：[`docs/uniubi_high_level_sdk.md`](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_high_level_sdk.md)
- 低级控制：[`docs/uniubi_low_level_sdk.md`](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_low_level_sdk.md)
- 媒体总线：[`docs/uniubi_media_sdk.md`](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_media_sdk.md)
- DDS / ROS 2 直连接入：[`docs/uniubi_robot_dds_api.md`](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_robot_dds_api.md)

首次真实机器人联调建议只执行 `standUp()` / `lieDown()`。`walking`、`move`、`bipedStand`、`handstand`、`jump*`、`damp` 等高风险运动动作应在空旷场地和人工接管条件下执行。

## 许可证

本仓库中的 UniUbi 原创代码、头文件、示例和文档使用 Apache License 2.0。预编译库和第三方组件按各自条款授权。详见 [LICENSE](LICENSE)、[NOTICE](NOTICE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
