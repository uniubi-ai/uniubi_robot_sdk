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

运行前确保动态链接器能找到 SDK 运行库：

```bash
export LD_LIBRARY_PATH=/path/to/uniubi-sdk/lib/$(uname -m):${LD_LIBRARY_PATH}
```

| 示例 | 行为 | 实机要求 |
|---|---|---|
| `example_highlevel` | High-level 交互 CLI：查询、传感器/里程计、取权、动作和参数控制 | 启动后不自动执行动作；控制命令仍要求空旷场地、急停可触达、有人值守 |
| `example_lowlevel` | 进入低级控制并周期下发控制帧 | 必须使用吊架，急停可触达 |
| `example_media_frames` | 板内订阅并落盘媒体帧 | 仅 aarch64，媒体服务和 SHM 已就绪 |

首次联调运行只读模式：

```bash
./build/examples/example_highlevel --read-only
```

进入 CLI 后使用 `status`、`odom 5`、`sensor 5` 做只读验证。退出只读模式、执行 `take` 后，`start`、`set`、`send`、`stop` 和 `estop` 才可用于控制机器人。程序不会自动启动动作，退出时会清零 walking 速度并释放控制权。

不要把 `example_lowlevel` 中的零目标、零增益模板当作平衡控制器。
