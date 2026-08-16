# C++ SDK Examples

[中文文档](README.zh-CN.md)

The examples can be built with the SDK source tree or independently against an installed SDK.

## Build with the Source Tree

```bash
cmake -S .. -B ../build
cmake --build ../build -j
```

A native build on JetPack 6.2.1 Orin also builds the C++ TensorRT Low-level example by default. It uses the system-provided CUDA 12.6 and TensorRT 10.3 and does not depend on PyTorch:

```bash
cmake -S .. -B ../build -DBUILD_SDK_TENSORRT_EXAMPLE=ON
cmake --build ../build --target example_lowlevel_tensorrt -j
```

## Cross-compile the TensorRT Example for Orin on Ubuntu x86_64

The validated environment is Ubuntu 22.04 x86_64 targeting JetPack 6.2.1 Orin. Install NVIDIA's cross packages from the dedicated `cross-linux-aarch64` repository:

```bash
wget https://developer.download.nvidia.com/compute/cuda/repos/ubuntu2204/cross-linux-aarch64/cuda-keyring_1.1-1_all.deb
sudo dpkg -i cuda-keyring_1.1-1_all.deb
sudo apt update
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
```

The repository's default TensorRT candidate may be newer than the version on the target. First create:

```text
# /etc/apt/preferences.d/uniubi-tensorrt-10-3-cross
Package: tensorrt-dev-cross-aarch64 libnvinfer*-cross-aarch64 libnvonnxparsers*-cross-aarch64
Pin: version 10.3.0.26-1+cuda12.5
Pin-Priority: 1001
```

Then install and build:

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

The cross dependencies download approximately 1.14 GB and occupy approximately 3.72 GB after installation. The SDK CMake configuration leaves Jetson target symbols such as `libnvdla_compiler.so` and `libcudla.so.1` to be resolved by the Orin runtime. Do not link the host's x86_64 TensorRT/CUDA libraries. See `uniubi-docs/docs/BUILD.md` for details.

## Build Against an Installed SDK

```bash
cmake -S . -B build -DCMAKE_PREFIX_PATH=/path/to/uniubi-sdk
cmake --build build -j
```

An SDK cross-built and installed for Orin must be used to build or run on Orin. Do not execute an aarch64 artifact on the x86_64 build host.

Before running, make the SDK runtime libraries visible to the dynamic linker:

```bash
case "$(uname -m)" in
  x86_64|amd64) SDK_ARCH=x86_64 ;;
  aarch64|arm64) SDK_ARCH=aarch64 ;;
  i386|i486|i586|i686) SDK_ARCH=i386 ;;
  *) echo "unsupported architecture"; exit 1 ;;
esac
export LD_LIBRARY_PATH="/path/to/uniubi-sdk/lib/$SDK_ARCH${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
```

On-board High-level, Low-level, and MediaBus examples require root privileges on current devices. Do not use only `sudo ./example` on-board, because `sudo` may remove `LD_LIBRARY_PATH`. External Linux x86_64 High-level discovery and client commands have been validated as a normal user.

| Example | Behavior | Hardware requirements |
|---|---|---|
| `example_highlevel` | Interactive High-level CLI for queries, sensors/odometry, ownership, actions, and parameters | Does not execute an action at startup; control commands still require a clear area, reachable emergency stop, and an attending operator |
| `example_lowlevel` | Interactive Low-level CLI for state, motor layout, damping, and pure-position stand/lie control | Does not move at startup; use a safety rig for initial enablement and posture control, with emergency stop reachable |
| `example_lowlevel_tensorrt` | Accepts ONNX, builds an FP32 TensorRT engine at every startup, and runs a Low-level policy at 50 Hz | Jetson Orin only; startup only connects; validate `stand` / `lay` on a rig, then `walk` on clear, level ground; emergency stop reachable and operator attending |
| `example_media_frames` | Subscribes to and saves on-board media frames | `aarch64` only; media service and SHM ready |

High-level can run either on the robot as an `aarch64` program or on an
external Linux x86_64 host. On-board single-device mode does not require an SN;
use the board High-level interface `eth0.100` for the first read-only run:

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel --iface eth0.100 --read-only
```

For an external host, check `ip -brief link` and enter the actual interface that
reaches the robot. Discover first; this mode is read-only and never selects a
device:

```bash
read -r -p "Host interface that reaches robot: " UNIUBI_IFACE
./build/examples/example_highlevel \
  --iface "$UNIUBI_IFACE" --discover-only
```

The callback is registered before SDK initialization. Results are collected
for five seconds and deduplicated by SN; a second five-second request is made
only if the first window received no callback.

There are two supported ways to obtain the target device ID:

1. Read the SN directly from the robot's **Basic Information** page in the Uniubi App.
2. Use SDK discovery. If it returns multiple robots and the target IP is known,
   inspect each result's `info` JSON and match the IP against
   `network.ether.ipv4Addr`, `network.wlan.ipv4Addr`,
   `network.hotspot.ipv4Addr`, or `network.mobile.ipv4Addr` to find its SN.

IP is only used to filter discovery results. Always pass the selected SN, not
the IP, to `--device-id`. Enter the SN exactly and pass both real values when
connecting:

```bash
read -r -p "Device SN from discovery: " UNIUBI_DEVICE_SN
./build/examples/example_highlevel \
  --iface "$UNIUBI_IFACE" --device-id "$UNIUBI_DEVICE_SN" --read-only
```

At the CLI, use `status`, `motors`, `odom 5`, and `sensor 5` for read-only validation:

```text
highlevel> status
highlevel> motors
highlevel> sensor 5
highlevel> odom 5
```

`--read-only` means that the program does not acquire control at startup. Run `take` explicitly when control is needed. To start directly in control mode, use `--iface eth0.100` on-board; an external host must retain both `--iface "$UNIUBI_IFACE"` and `--device-id "$UNIUBI_DEVICE_SN"`. For example:

```text
highlevel> take
highlevel> start walking {"lineVelocityX":0.0,"lineVelocityY":0.0,"velocity":0.0}
highlevel> send 3 {"lineVelocityX":0.3,"lineVelocityY":0,"velocity":0}
highlevel> stop
highlevel> release
highlevel> quit
```

The program never starts an action automatically. When `send` expires, it zeros walking velocity; normal exit also zeros velocity and releases ownership. Enter `help` for the complete command set, including `set`, `zero`, and `estop`.

The Low-level CLI only connects at startup and does not enable control. Use this validation sequence:

> **Before running, secure the robot on a reliable safety rig with all four feet fully clear. Ensure every leg can move freely without touching the floor, rig, or nearby objects. Keep the emergency stop within reach and have a dedicated operator attend the robot.**

```text
lowlevel> status
lowlevel> motors
lowlevel> damping
lowlevel> stand
lowlevel> lie
lowlevel> release
lowlevel> quit
```

Low-level has no `take` / `startControl` wrapper. `stand`, `lie`, and `damping` call `setMotionEnable(true)` as needed. `stand` / `lie` use the standard DV500 12-joint posture parameters, interpolate smoothly from live joint positions for two seconds, and then hold the posture. The program refuses to enable control when the layout does not match. Both `Ctrl+C` and `quit` release control and attempt to restore built-in motion control. This example may only be tested with all four feet suspended; do not run it directly on the ground.

## C++ TensorRT Low-level Policy Example

`example_lowlevel_tensorrt` accepts a static `[1,45] -> [1,12]` ONNX model and rebuilds an FP32 TensorRT engine at every process startup. It neither reads nor writes an engine cache. At runtime it depends only on JetPack's TensorRT/CUDA C++ runtime and the SDK; it does not depend on PyTorch or ONNX Runtime.

First validate the build and one zero-input inference without initializing the SDK or enabling motors:

```bash
taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx --validate-only
```

Hardware execution requires root privileges. Pinning the process to CPU 2 is recommended to reduce scheduler jitter and stabilize observation latency and the 50 Hz control period:

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx
```

After connecting, the program calls `getMotorLayout()` and requires exactly 12 joints in this leg-major order:

```text
FL_ABAD, FL_HIP, FL_KNEE,
FR_ABAD, FR_HIP, FR_KNEE,
RL_ABAD, RL_HIP, RL_KNEE,
RR_ABAD, RR_HIP, RR_KNEE
```

The example model uses joint-major input and output order. The program explicitly performs the bidirectional `SDK leg-major -> model joint-major -> SDK leg-major` reorder and constructs control frames with the actual `MotorInfo.limbNo` / `jointNo`. It exits before enablement if the count or order does not match.

Validate hardware motion in two stages. First secure the robot on a safety rig with all four feet fully clear and execute only:

```text
lowlevel> stand
lowlevel> lay
lowlevel> quit
```

After confirming posture, joint directions, and emergency stop operation, place the robot on clear, level, obstacle-free ground and execute:

```text
lowlevel> stand
lowlevel> walk 0.5 0 0
lowlevel> stop
lowlevel> lay
lowlevel> quit
```

Do not execute `walk` with all four feet suspended. During both stages, keep the emergency stop within reach and have a dedicated operator attend the robot.

On exit, this policy example calls `setMotionEnable(false)` only if it is already in the prepared state, then disconnects and shuts down the SDK. It does not call `emergencyStop()` or `restoreMotionControlMode()`. This exit behavior differs from the generic `example_lowlevel` above and must not be conflated with it.

The example is disabled by default for cross-compilation. When `-DBUILD_SDK_TENSORRT_EXAMPLE=ON` is set explicitly, `UNIUBI_TENSORRT_ROOT` and `UNIUBI_CUDA_ROOT` must provide aarch64 headers and link libraries that match the target JetPack. Both the NVIDIA APT cross-package path above and native Orin build path have been validated. The SDK repository does not redistribute NVIDIA binary libraries.
