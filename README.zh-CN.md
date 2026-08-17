# Uniubi Robot SDK

[English](README.md)

Uniubi 机器人 C++ 运动控制 SDK 的公开开发仓库，提供公开头文件、C++ 示例、构建入口和按架构交付的预编译运行库。完整开发路径与 API 说明统一维护在 [`uniubi-docs`](https://github.com/uniubi-ai/uniubi-docs)。

## 编译与安装

本仓库已经包含预编译的 `librobotMotionSdk.so` 及配套运行库。这里的 CMake 工程用于编译 C++ 示例和验证开发环境，不会重新编译 SDK 运行库。

### 1. 确定目标架构和构建方式

SDK 架构由**程序最终运行的机器**决定：

| 程序最终运行位置 | 目标架构 | 典型用途 | 可选构建方式 |
|---|---|---|---|
| Uniubi 提供的 Orin 开发板 | `aarch64` | Low-level、MediaBus、板内 High-level | 登录 Orin 直接构建，或在 x86_64 Linux 主机交叉编译 |
| x86_64 Linux 主机 | `x86_64` | 远端 High-level、Mock / 联调工具 | 在该 x86_64 主机上直接构建 |
| 32 位 x86 Linux 设备 | `i386` | 特定存量系统 | 在目标设备构建，或使用对应交叉工具链 |

如果程序最终运行在 Orin 上，目标架构始终是 `aarch64`。开发者可以选择：

1. 登录 Uniubi 提供的 Orin 开发板，在 Orin 上原生构建；
2. 在 x86_64 Linux 主机上交叉编译，再将 `aarch64` 产物部署到 Orin。

Low-level 和 MediaBus 是板内能力，但不限制采用原生构建还是交叉编译。

### 2. 准备编译环境

| 项目 | 要求 |
|---|---|
| 操作系统 | Linux |
| glibc | 2.34 或更高 |
| 编译器 | g++ 9 或更高，支持 C++14 |
| CMake | 3.18 或更高 |

公开头文件和 `lib/` 下的运行库必须来自同一套 SDK 交付版本，不能跨版本或跨架构混用。

### 3. 获取 SDK

在编译机器上执行：

```bash
git clone https://github.com/uniubi-ai/uniubi_robot_sdk.git
cd uniubi_robot_sdk
export UNIUBI_SDK_ROOT="$PWD"
```

### 4. 选择一种编译方式

#### 方式 A：在最终运行机器上直接编译

在 Orin、x86_64 Linux 主机或对应目标设备上执行：

```bash
cmake -S . -B build
cmake --build build -j
```

CMake 根据当前机器的 `CMAKE_SYSTEM_PROCESSOR` 自动选择运行库：

- Orin：`lib/aarch64/`；
- x86_64 Linux：`lib/x86_64/`；
- 32 位 x86：`lib/i386/`。

#### 方式 B：在 x86_64 Linux 主机上为 Orin 交叉编译

安装 `aarch64` 交叉编译器后，使用仓库提供的工具链文件：

```bash
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake
cmake --build build-aarch64 -j
```

交叉编译产物位于 `build-aarch64/examples/`，需要部署到 Orin 上运行。自定义 sysroot、工具链和安装前缀见 [完整构建指南](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.zh-CN.md)。

交叉编译 `example_lowlevel_tensorrt` 还需要 NVIDIA 的 aarch64 CUDA/TensorRT 开发
包，并将 TensorRT 固定为与 JetPack 6.2.1 对应的 10.3。不能直接使用软件源中的
默认最新 TensorRT，也不能链接主机 x86_64 NVIDIA 库。已验证的专用软件源、版本
pin 和完整 CMake 命令见 [构建指南 §3.1](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.zh-CN.md#31-交叉编译-tensorrt-示例的额外边界)。

### 5. 安装 SDK（推荐）

将当前目标架构的公开头、运行库、CMake package 和示例程序安装到一个前缀：

```bash
cmake --install build --prefix "$HOME/.local/uniubi"
```

如果是在 x86_64 Linux 主机上为 Orin 交叉编译，则安装对应的交叉编译目录：

```bash
cmake --install build-aarch64 --prefix "$HOME/.local/uniubi-aarch64"
```

安装后，业务工程可以直接使用导出的 CMake target：

```cmake
find_package(UniubiRobotSdk CONFIG REQUIRED)
target_link_libraries(my_robot_app PRIVATE Uniubi::RobotMotionSdk)
# MediaBus 应用额外链接 Uniubi::MediaBus
```

配置业务工程时通过 `-DCMAKE_PREFIX_PATH=$HOME/.local/uniubi` 指定安装前缀。安装布局为 `include/`、`lib/<arch>/`、`lib/cmake/UniubiRobotSdk/` 和 `bin/`。交叉安装目录中的程序和运行库需要一起部署到 Orin，不能在编译主机上运行。

如果只想在源码树内开发，也仍可把仓库根目录直接作为 `UNIUBI_SDK_ROOT`。

### 6. 配置运行库并运行示例

构建过程不需要 `sudo`。当前设备上的板载 High-level、Low-level 和 MediaBus 程序需要 root 权限，运行时使用 `sudo env` 保留 `LD_LIBRARY_PATH`；外部 Linux x86_64 High-level client 已验证可由普通用户运行，不应笼统描述为必须 root。

如果使用了第 5 节的安装方式，下面展示板载 `aarch64` 单设备形式，并显式使用板载
High-level 网卡 `eth0.100`：

```bash
export UNIUBI_SDK_PREFIX="$HOME/.local/uniubi"

case "$(uname -m)" in
  x86_64|amd64) SDK_ARCH=x86_64 ;;
  aarch64|arm64) SDK_ARCH=aarch64 ;;
  i386|i486|i586|i686) SDK_ARCH=i386 ;;
  *) echo "Unsupported architecture: $(uname -m)"; exit 1 ;;
esac

export LD_LIBRARY_PATH="$UNIUBI_SDK_PREFIX/lib/$SDK_ARCH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  "$UNIUBI_SDK_PREFIX/bin/example_highlevel" --iface eth0.100 --read-only
```

如果直接从源码树运行，在 SDK 仓库根目录执行：

```bash
case "$(uname -m)" in
  x86_64|amd64) SDK_ARCH=x86_64 ;;
  aarch64|arm64) SDK_ARCH=aarch64 ;;
  i386|i486|i586|i686) SDK_ARCH=i386 ;;
  *) echo "Unsupported architecture: $(uname -m)"; exit 1 ;;
esac

export UNIUBI_SDK_ROOT="$PWD"
export LD_LIBRARY_PATH="$UNIUBI_SDK_ROOT/lib/$SDK_ARCH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel --iface eth0.100 --read-only
```

直接编译的示例位于 `build/examples/`；交叉编译的示例位于 `build-aarch64/examples/`。`example_media_frames` 在 `aarch64` 目标上默认构建；`example_lowlevel_tensorrt` 只在 Orin 原生构建时默认启用，交叉编译需显式提供目标端 TensorRT/CUDA 开发文件。

### 7. 未安装时集成到自己的 CMake 项目

```cmake
set(UNIUBI_SDK_ROOT "$ENV{UNIUBI_SDK_ROOT}" CACHE PATH "Uniubi SDK root")

if(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  set(ARCH_DIR x86_64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  set(ARCH_DIR aarch64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i.86|x86)$")
  set(ARCH_DIR i386)
else()
  message(FATAL_ERROR "Unsupported architecture: ${CMAKE_SYSTEM_PROCESSOR}")
endif()

find_library(UNIUBI_MOTION_SDK robotMotionSdk
  PATHS ${UNIUBI_SDK_ROOT}/lib/${ARCH_DIR}
  NO_DEFAULT_PATH REQUIRED)

target_include_directories(my_robot_app PRIVATE
  ${UNIUBI_SDK_ROOT}/include)
target_link_libraries(my_robot_app PRIVATE ${UNIUBI_MOTION_SDK} pthread)

if(CMAKE_CROSSCOMPILING AND ARCH_DIR STREQUAL "aarch64")
  get_filename_component(UNIUBI_SDK_LIBRARY_DIR
    "${UNIUBI_MOTION_SDK}" DIRECTORY)
  target_link_options(my_robot_app PRIVATE
    "-Wl,-rpath-link,${UNIUBI_SDK_LIBRARY_DIR}"
    "-Wl,--allow-shlib-undefined")
endif()
```

运行时还必须保证同架构、同版本的 `libmediaBus.so`、`libudbus.so`、`libubase.so` 及其配套依赖可以被动态链接器找到。

## 选择开发路径

完成编译和运行库配置后，再按开发目标选择 SDK 入口：

| 你要做什么 | 控制模式 | SDK 入口 | 先看哪里 |
|---|---|---|---|
| 查询状态、传感器或里程计 | High-level 只读数据 | `MotionHighLevelClient` | [`example_highlevel.cpp`](examples/example_highlevel.cpp) 的 `--read-only` 模式 |
| 使用机器人内置站立、趴下、行走等动作 | High-level | `MotionHighLevelClient` | [`example_highlevel.cpp`](examples/example_highlevel.cpp) 交互 CLI |
| 自己运行策略，直接控制关节位置或扭矩 | Low-level | `MotionLowLevelClient` | 姿态控制见 [`example_lowlevel.cpp`](examples/example_lowlevel.cpp)；ONNX/TensorRT 策略见 [`example_lowlevel_tensorrt.cpp`](examples/example_lowlevel_tensorrt.cpp) |
| 订阅摄像头、麦克风或编码帧 | MediaBus | `MediaBusClient` | [`example_media_frames.cpp`](examples/example_media_frames.cpp) |

- **High-level**：应用发送动作或运动意图，由机器人内置能力完成关节级闭环。
- **Low-level**：应用自己运行策略或控制器，并周期性生成关节位置或扭矩控制量。

`queryMotionState()` 查询的是当前活动 action，不是机器人姿态或电机是否就绪。只有存在活动 action 时，返回对象才包含 `action`、速度等状态字段；没有活动 action 时，调用仍成功，但输出为标准空对象 `{}`。连接或 RPC 失败则返回 `false`，调用方应通过 `getLastError()` 获取原因。

如果还没有确定控制模式，先阅读 [`uniubi-docs` Quick Start](https://github.com/uniubi-ai/uniubi-docs/blob/main/README.zh-CN.md#quick-start)。

## 能力与部署边界

| 能力 | 板内单设备 | 外部主机（按设备寻址） | 关键限制 |
|---|---:|---:|---|
| High-level 控制与观测 | 支持 | 支持 | 外部主机即使只访问一台机器人，也必须选择网卡并使用设备 SN 创建 client |
| Low-level 关节控制 | 支持 | 不支持 | 直连本地 MotionServer，数据面使用 SHM |
| MediaBus 帧订阅 | 仅 `aarch64` | 不支持 | 依赖板内媒体服务、配置和 SHM |

`example_highlevel` 支持两种部署：可将 `aarch64` 程序放在机器人板内按单设备方式运行，
也可在外部 Linux x86_64 主机运行 `x86_64` 程序进行远端 High-level 访问。外部访问必须
使用能到达机器人的主机网卡和显式目标 SN，程序不会自动选择发现到的第一台设备。详见
[High-level SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_high_level_sdk.zh-CN.md)。
SDK 内部的多设备能力不是另一种部署形态，而是用于支持外部主机发现设备并按 SN 创建 client。

## 运行示例

以下命令以目标机器直接编译生成的 `build/` 目录为例。交叉编译时，应将 `build-aarch64/examples/` 中的程序部署到 Orin，并确保 Orin 使用同一套 `lib/aarch64/` 运行库。

当前设备上的板载 High-level、Low-level 和 MediaBus 示例需要 root 权限，命令使用 `sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH"`；外部 Linux x86_64 High-level 发现与 client 命令使用已导出的运行库路径，以普通用户运行。

### High-level CLI：选择板载或外部 Linux 运行

`example_highlevel` 是交互式 High-level 工具，启动后不会自动执行动作。首次连接使用 `--read-only`，不申请运动控制权：

在机器人板内运行（`aarch64`、单设备）时，SDK 创建本地 client，不需要设备 SN，显式
使用板载 High-level 网卡 `eth0.100`：

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel --iface eth0.100 --read-only
```

在外部 Linux x86_64 主机运行时，先用 `ip -brief link` 核对并输入能到达机器人的真实
网卡，再执行纯只读发现：

```bash
read -r -p "Host interface that reaches robot: " UNIUBI_IFACE
./build/examples/example_highlevel \
  --iface "$UNIUBI_IFACE" --discover-only
```

发现回调在 SDK 初始化前注册；程序收集 5 秒并按 SN 去重，只有首个窗口完全没有回调时
才重试一次。发现模式不创建机器人 client、不取权，也不会自动选设备。

目标 device ID 有两种获取方式：

1. 在 Uniubi App 中打开机器人的“基础信息”页面，直接查看 SN。
2. 使用上面的 SDK discovery。若发现结果包含多台机器人且已知目标 IP，检查每条结果的
   `info` JSON，用该 IP 匹配 `network.ether.ipv4Addr`、`network.wlan.ipv4Addr`、
   `network.hotspot.ipv4Addr` 或 `network.mobile.ipv4Addr`，由此找到对应 SN。

IP 只用于筛选 discovery 结果；`--device-id` 最终始终传目标机器人的 SN，不能传 IP。
输入真实 SN 后，再同时传入真实网卡和显式 device ID 进行只读连接：

```bash
read -r -p "Device SN from discovery: " UNIUBI_DEVICE_SN
./build/examples/example_highlevel \
  --iface "$UNIUBI_IFACE" --device-id "$UNIUBI_DEVICE_SN" --read-only
```

进入 CLI 后会显示 `highlevel>` 提示符，可以先完成只读检查：

```text
highlevel> status
highlevel> caps
highlevel> motors
highlevel> sensor 5
highlevel> odom 5
highlevel> quit
```

`odom` 已合并到 High-level CLI。里程计只在机器人处于 Walk 动作期间有效；`position` 已由设备端累计，上层不要再次积分。

### High-level 控制验证

真机申请控制权前，必须先关闭遥控器，或长按遥控器 `M` 键切换，直到听到“遥控器连接已断开”
的语音提示。遥控器仍连接时，High-level 无法取得控制权；只读检查不要求断开遥控器。

需要控制机器人时，不加 `--read-only` 启动。程序会获取 High-level 控制权，但不会自动启动动作。板载命令为：

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel --iface eth0.100
```

外部 Linux 主机必须继续显式传入两个真实值：

```bash
./build/examples/example_highlevel \
  --iface "$UNIUBI_IFACE" --device-id "$UNIUBI_DEVICE_SN"
```

默认模式连接成功后会自动申请控制权，但不会自动启动动作。`--read-only` 只表示启动时不申请控制权；进入 CLI 后仍可显式执行 `take`。从 `--read-only` 会话显式取权并完成一次限时行走的流程如下：

```text
highlevel> take
highlevel> start walking {"lineVelocityX":0.0,"lineVelocityY":0.0,"velocity":0.0}
highlevel> send 3 {"lineVelocityX":0.3,"lineVelocityY":0,"velocity":0}
highlevel> stop
highlevel> release
highlevel> quit
```

`start ACTION [JSON]` 启动动作，`set JSON` 持续设置动作参数，`send SECONDS JSON` 限时发送参数并在结束后自动清零 walking 速度。`zero` 只清零 walking 速度、不会结束动作；`stop` 会停止当前动作，将实际动作切回零速 `walking` 并继续保留控制权。显式启动三个参数均为 0 的 `walking` 也可以完成同样的动作切换；`release` 释放控制权，`estop` 请求急停。随时输入 `help` 查看程序当前支持的完整命令。

首次实机运行前必须确保场地空旷、急停可触达并有人值守。外部连接不得依赖默认网卡，也不得省略 `--device-id`。

### Low-level 关节控制验证

> **测试前必须将机器狗可靠吊起，使四脚完全腾空，并确保四肢在运动范围内能够自由活动、不会碰到地面、吊架或周围物体；同时保持急停可触达并由专人值守。**

`example_lowlevel` 是交互式 Low-level 工具。启动后只建立连接，不使能电机控制，也不会自动执行姿态：

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_lowlevel
```

进入 CLI 后先检查状态和电机布局，再执行姿态命令：

```text
lowlevel> status
lowlevel> motors
lowlevel> stand
lowlevel> lie
lowlevel> damping
lowlevel> release
lowlevel> quit
```

`stand`、`lie` 和 `damping` 会按需调用 `setMotionEnable(true)`，CLI 不额外包装 High-level 风格的 `take` 命令。`stand` 和 `lie` 从实时关节位置开始，以 50 Hz 在 2 秒内平滑移动到目标姿态并持续保持。`damping` 清零位置刚度并保留速度阻尼，`release` 先进入阻尼再关闭 Low-level 控制，`quit` 还会尝试恢复机器人内置运控。

该示例使用标准 DV500 12 关节布局和板端已验证的姿态参数；布局不匹配时会拒绝使能姿态控制。Low-level 示例测试期间必须始终保持机器狗四脚腾空且能够自由活动，不得直接放在地面执行。接口状态要求见 [Low-level SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_low_level_sdk.zh-CN.md)。

### Low-level TensorRT 策略验证（Jetson Orin）

JetPack 6.2.1 Orin 原生构建默认启用 `example_lowlevel_tensorrt`。该示例输入静态
`[1,45] -> [1,12]` ONNX，每次启动都重新构建 FP32 TensorRT engine，不缓存
engine，也不依赖 PyTorch 或 ONNX Runtime。

实机动作分两阶段验证。首先将机器狗可靠固定在安全吊架上，保持四脚完全腾空，只验证站立和趴下：

```text
lowlevel> stand
lowlevel> lay
lowlevel> quit
```

确认姿态、关节方向和急停均正常后，将机器狗放到空旷、平整、无障碍地面，再验证策略行走：

```text
lowlevel> stand
lowlevel> walk 0.5 0 0
lowlevel> stop
lowlevel> lay
lowlevel> quit
```

不要在四脚腾空时执行 `walk`；两个阶段都必须保持急停可触达并由专人值守。

```bash
cmake -S . -B build -DBUILD_SDK_TENSORRT_EXAMPLE=ON
cmake --build build --target example_lowlevel_tensorrt -j$(nproc)

# 只构建 engine 并做零输入推理，不初始化 SDK
taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx --validate-only

# 实机交互；建议绑定 CPU 2，减少调度抖动
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx
```

程序在使能前通过 `getMotorLayout()` 校验 12 关节 leg-major 布局，并按照实际
`limbNo` / `jointNo` 构造控制帧。示例模型使用 joint-major 输入输出顺序，因此代码
显式执行 `SDK leg-major -> 模型 joint-major -> SDK leg-major` 双向重排；布局、
关节数或模型 shape 不匹配时拒绝控制。

退出只在必要时调用 `setMotionEnable(false)`，随后断开并关闭 SDK；不调用
`emergencyStop()` 或 `restoreMotionControlMode()`。完整模型契约、命令和交叉编译
依赖说明见 [`examples/README.md`](examples/README.md)。

### MediaBus 验证

`example_media_frames` 仅用于 `aarch64` 板内本地媒体帧订阅。运行前需要确认 `/etc/robot/sdk_config.json`、媒体服务和 SHM 环境已经就绪，具体参数和排查方法见 [故障排查](docs/troubleshooting.zh-CN.md)。

## 示例索引

| 示例 | 是否申请控制权 | 是否可能引发运动 | 用途 |
|---|---:|---:|---|
| `example_highlevel --iface IFACE --discover-only` | 否 | 否 | 发现远端设备、按 SN 去重，不选设备并直接退出 |
| `example_highlevel --read-only` | 否 | 否 | High-level 状态、能力、传感器和 Walk 里程计查询 |
| `example_highlevel` | 是 | 只有显式输入控制命令后才会 | High-level 交互 CLI；保持控制租约并分步测试动作和参数 |
| `example_lowlevel` | 姿态/阻尼命令按需使能 | `stand` / `lie` 会 | Low-level 交互 CLI；状态检查、阻尼和站立/趴下纯位置控制 |
| `example_lowlevel_tensorrt` | `stand` / `walk` 按需使能 | `stand` / `walk` / `stop` / `lay` 会 | Orin 板内 ONNX -> FP32 TensorRT Low-level 策略；每次启动重新 build |
| `example_media_frames` | 否 | 否 | 板内音视频帧订阅和落盘 |

## 文档导航

- [故障排查](docs/troubleshooting.zh-CN.md)
- [`uniubi-docs` 开发入口](https://github.com/uniubi-ai/uniubi-docs)
- [构建、安装和交叉编译](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.zh-CN.md)
- [High-level C++ API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/api-reference/cpp/high-level.zh-CN.md)
- [Low-level C++ API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/api-reference/cpp/low-level.zh-CN.md)
- [MediaBus C++ API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/api-reference/cpp/media.zh-CN.md)
- [核心概念](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/core-concepts/README.zh-CN.md)

DDS / ROS 2 协议直连属于 Advanced 集成路径，不是普通 C++ SDK 开发入口；需要时从 [`uniubi-docs` Advanced](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/advanced/README.zh-CN.md) 进入。

## 许可证

本仓库中的 UniUbi 原创代码、头文件、示例和文档使用 Apache License 2.0。预编译库和第三方组件按各自条款授权。详见 [LICENSE](LICENSE)、[NOTICE](NOTICE) 和 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。
