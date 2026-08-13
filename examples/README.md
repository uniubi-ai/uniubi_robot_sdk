# C++ SDK 示例

示例可以随 SDK 源码树构建，也可以只依赖已经安装的 SDK 独立构建。

## 随源码构建

```bash
cmake -S .. -B ../build
cmake --build ../build -j
```

在 JetPack 6.2.1 Orin 上原生构建时，会默认同时构建 C++ TensorRT Low-level
示例。它使用系统预装的 CUDA 12.6 和 TensorRT 10.3，不依赖 PyTorch：

```bash
cmake -S .. -B ../build -DBUILD_SDK_TENSORRT_EXAMPLE=ON
cmake --build ../build --target example_lowlevel_tensorrt -j
```

## 在 Ubuntu x86_64 上为 Orin 交叉编译 TensorRT 示例

已验证环境为 Ubuntu 22.04 x86_64 → JetPack 6.2.1 Orin。NVIDIA 交叉包必须从
`cross-linux-aarch64` 专用软件源安装：

```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/cross-linux-aarch64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

当前软件源的 TensorRT 默认 candidate 可能高于目标机版本。先创建：

```text
# /etc/apt/preferences.d/uniubi-tensorrt-10-3-cross
Package: tensorrt-dev-cross-aarch64 libnvinfer*-cross-aarch64 libnvonnxparsers*-cross-aarch64
Pin: version 10.3.0.26-1+cuda12.5
Pin-Priority: 1001
```

再安装并构建：

```bash
sudo apt install cuda-cross-aarch64-12-6 tensorrt-dev-cross-aarch64

cmake -S .. -B ../build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake \
  -DBUILD_SDK_TENSORRT_EXAMPLE=ON \
  -DBUILD_SDK_MEDIA_EXAMPLE=OFF \
  -DUNIUBI_TENSORRT_ROOT=/usr \
  -DUNIUBI_CUDA_ROOT=/usr/local/cuda-12.6 \
  -DCMAKE_BUILD_TYPE=Release
cmake --build ../build-aarch64 --target example_lowlevel_tensorrt -j$(nproc)
```

交叉依赖下载约 1.14 GB、安装后约占 3.72 GB。SDK CMake 会保留
`libnvdla_compiler.so`、`libcudla.so.1` 等 Jetson 目标端符号，由 Orin 运行时解析；
不要链接主机 x86_64 的 TensorRT/CUDA 库。详细说明见 `uniubi-docs/docs/BUILD.md`。

## 针对已安装 SDK 构建

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/uniubi-sdk
cmake --build build -j
```

交叉编译安装到 Orin 的 SDK，需要在 Orin 上使用该安装前缀构建或运行；不要在 x86_64 编译主机上运行 aarch64 产物。

运行前确保动态链接器能找到 SDK 运行库：

```bash
case "$(uname -m)" in
  x86_64|amd64) SDK_ARCH=x86_64 ;;
  aarch64|arm64) SDK_ARCH=aarch64 ;;
  i386|i486|i586|i686) SDK_ARCH=i386 ;;
  *) echo "unsupported architecture"; exit 1 ;;
esac
export LD_LIBRARY_PATH="/path/to/uniubi-sdk/lib/$SDK_ARCH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

当前设备运行 SDK 示例需要 root 权限。不要只写 `sudo ./example`，否则 `sudo` 可能清理 `LD_LIBRARY_PATH`；以下命令显式传入运行库路径。

| 示例 | 行为 | 实机要求 |
|---|---|---|
| `example_highlevel` | High-level 交互 CLI：查询、传感器/里程计、取权、动作和参数控制 | 启动后不自动执行动作；控制命令仍要求空旷场地、急停可触达、有人值守 |
| `example_lowlevel` | Low-level 交互 CLI：状态、电机布局、阻尼及站立/趴下纯位置控制 | 启动不动作；首次使能和姿态控制建议先使用吊架，急停可触达 |
| `example_lowlevel_tensorrt` | 输入 ONNX，每次启动现场构建 FP32 TensorRT engine，并以 50 Hz 运行 Low-level 策略 | 仅 Jetson Orin；启动只连接，执行 `stand` / `walk` 后才使能；必须可靠吊起并有人值守 |
| `example_media_frames` | 板内订阅并落盘媒体帧 | 仅 aarch64，媒体服务和 SHM 已就绪 |

首次联调运行只读模式：

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel --read-only
```

进入 CLI 后使用 `status`、`motors`、`odom 5`、`sensor 5` 做只读验证：

```text
highlevel> status
highlevel> motors
highlevel> sensor 5
highlevel> odom 5
```

`--read-only` 表示启动时不申请控制权；需要控制时显式执行 `take`。例如：

```text
highlevel> take
highlevel> start walking
highlevel> send 3 {"lineVelocityX":0.3,"lineVelocityY":0,"velocity":0}
highlevel> stop
highlevel> release
highlevel> quit
```

程序不会自动启动动作。`send` 到时后会清零 walking 速度；正常退出时也会清零速度并释放控制权。输入 `help` 可查看 `set`、`zero`、`estop` 等完整命令。

Low-level CLI 启动后只连接，不使能控制。推荐按下面顺序验证：

> **运行前必须将机器狗可靠吊起，使四脚完全腾空，并确保四肢能够自由活动且不会碰到地面、吊架或周围物体。测试过程中保持急停可触达并由专人值守。**

```text
lowlevel> status
lowlevel> motors
lowlevel> damping
lowlevel> stand
lowlevel> lie
lowlevel> release
lowlevel> quit
```

Low-level 没有 `take` / `startControl` 接口包装；`stand`、`lie` 和 `damping` 内部按需调用 `setMotionEnable(true)`。`stand` / `lie` 使用标准 DV500 12 关节姿态参数，从实时关节位置平滑插值 2 秒后持续保持。布局不匹配时程序会拒绝使能。`Ctrl+C` 和 `quit` 都会执行释放并尝试恢复内置运控。该示例只允许在四脚腾空条件下测试，不得直接落地运行。

## C++ TensorRT Low-level 策略示例

`example_lowlevel_tensorrt` 接收静态 `[1,45] -> [1,12]` ONNX 模型，每次进程启动
都重新构建 FP32 TensorRT engine，不读取或写入 engine 缓存。运行时只依赖 JetPack
自带的 TensorRT/CUDA C++ 运行库和 SDK，不依赖 PyTorch、ONNX Runtime。

先做不初始化 SDK、不使能电机的构建与零输入推理验证：

```bash
taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx --validate-only
```

实机运行需要 root 权限，并建议绑定 CPU 2，以减少调度抖动，使观测获取耗时和
50 Hz 控制周期更稳定：

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx
```

程序连接后先调用 `getMotorLayout()`，要求实际布局恰好为 12 关节 leg-major 顺序：

```text
FL_ABAD, FL_HIP, FL_KNEE,
FR_ABAD, FR_HIP, FR_KNEE,
RL_ABAD, RL_HIP, RL_KNEE,
RR_ABAD, RR_HIP, RR_KNEE
```

示例模型的输入输出是 joint-major 顺序，程序显式执行
`SDK leg-major -> 模型 joint-major -> SDK leg-major` 双向重排，并使用实际
`MotorInfo.limbNo` / `jointNo` 构造控制帧。数量或顺序不匹配时会在使能前退出。

实机命令流程：

```text
lowlevel> stand
lowlevel> walk 0.5 0 0
lowlevel> stop
lowlevel> lay
lowlevel> quit
```

该策略示例退出时仅在已经处于 prepared 状态时调用 `setMotionEnable(false)`，随后
断开连接并关闭 SDK；不会调用 `emergencyStop()` 或 `restoreMotionControlMode()`。
这一退出语义与上面的通用 `example_lowlevel` 不同，不应混写。

交叉编译时默认不构建该示例。显式设置
`-DBUILD_SDK_TENSORRT_EXAMPLE=ON` 后，还必须通过 `UNIUBI_TENSORRT_ROOT` 和
`UNIUBI_CUDA_ROOT` 提供与目标 JetPack 匹配的 aarch64 头文件和链接库。上面的
NVIDIA APT 交叉包路径和 Orin 原生构建路径均已验证；SDK 仓库不分发 NVIDIA
二进制库。
