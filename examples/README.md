# C++ SDK 示例

示例可以随 SDK 源码树构建，也可以只依赖已经安装的 SDK 独立构建。

## 随源码构建

```bash
cmake -S .. -B ../build
cmake --build ../build -j
```

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

| 示例 | 行为 | 实机要求 |
|---|---|---|
| `example_highlevel` | High-level 交互 CLI：查询、传感器/里程计、取权、动作和参数控制 | 启动后不自动执行动作；控制命令仍要求空旷场地、急停可触达、有人值守 |
| `example_lowlevel` | Low-level 交互 CLI：状态、电机布局、阻尼及站立/趴下纯位置控制 | 启动不动作；首次使能和姿态控制建议先使用吊架，急停可触达 |
| `example_media_frames` | 板内订阅并落盘媒体帧 | 仅 aarch64，媒体服务和 SHM 已就绪 |

首次联调运行只读模式：

```bash
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
