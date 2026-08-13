# Uniubi Robot SDK

[中文文档](README.zh-CN.md)

The public development repository for the Uniubi Robot C++ motion-control SDK. It provides public headers, C++ examples, build entry points, and prebuilt runtime libraries for each supported architecture. The complete development workflow and API documentation are maintained in [`uniubi-docs`](https://github.com/uniubi-ai/uniubi-docs).

## Build and Install

This repository contains prebuilt `librobotMotionSdk.so` and its companion runtime libraries. The CMake project builds the C++ examples and validates the development environment; it does not rebuild the SDK runtime libraries.

### 1. Choose the target architecture and build method

The SDK architecture is determined by the **machine on which the program will run**:

| Final runtime location | Target architecture | Typical use | Available build methods |
|---|---|---|---|
| Uniubi-provided Orin development board | `aarch64` | Low-level, MediaBus, and on-board High-level | Build natively on Orin, or cross-compile on an x86_64 Linux host |
| x86_64 Linux host | `x86_64` | Remote High-level, Mock, and integration tools | Build directly on that x86_64 host |
| 32-bit x86 Linux device | `i386` | Specific legacy systems | Build on the target device or use a matching cross toolchain |

If the program will run on Orin, the target architecture is always `aarch64`. You can either:

1. log in to the Uniubi-provided Orin board and build natively; or
2. cross-compile on an x86_64 Linux host and deploy the `aarch64` artifacts to Orin.

Low-level and MediaBus are on-board capabilities, but they do not require one particular build method.

### 2. Prepare the build environment

| Item | Requirement |
|---|---|
| Operating system | Linux |
| glibc | 2.34 or later |
| Compiler | g++ 9 or later with C++14 support |
| CMake | 3.18 or later |

The public headers and libraries under `lib/` must come from the same SDK delivery. Do not mix versions or architectures.

### 3. Get the SDK

Run on the build machine:

```bash
git clone https://github.com/uniubi-ai/uniubi_robot_sdk.git
cd uniubi_robot_sdk
export UNIUBI_SDK_ROOT="$PWD"
```

### 4. Select a build method

#### Option A: build on the final runtime machine

Run on Orin, an x86_64 Linux host, or the corresponding target device:

```bash
cmake -S . -B build
cmake --build build -j
```

CMake selects the runtime directory from the current machine's `CMAKE_SYSTEM_PROCESSOR`:

- Orin: `lib/aarch64/`
- x86_64 Linux: `lib/x86_64/`
- 32-bit x86: `lib/i386/`

#### Option B: cross-compile for Orin on an x86_64 Linux host

Install an `aarch64` cross-compiler and use the toolchain file provided by this repository:

```bash
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

cmake -S . -B build-aarch64 \
  -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-aarch64-linux-gnu.cmake
cmake --build build-aarch64 -j
```

The artifacts are written to `build-aarch64/examples/` and must run on Orin. See the [complete build guide](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.md) for custom sysroots, toolchains, and install prefixes.

Cross-compiling `example_lowlevel_tensorrt` also requires NVIDIA's aarch64 CUDA/TensorRT development packages. TensorRT must be pinned to 10.3, which matches JetPack 6.2.1. Do not use the repository's latest default TensorRT package and do not link NVIDIA x86_64 host libraries. See the [Build Guide](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.md) for the validated cross repository, version pin, and complete CMake command.

### 5. Install the SDK (recommended)

Install the public headers, runtime libraries for the current target architecture, CMake package, and example programs into one prefix:

```bash
cmake --install build --prefix "$HOME/.local/uniubi"
```

When cross-compiling for Orin on an x86_64 Linux host, install the cross-build directory instead:

```bash
cmake --install build-aarch64 --prefix "$HOME/.local/uniubi-aarch64"
```

Consumer projects can then use the exported CMake targets:

```cmake
find_package(UniubiRobotSdk CONFIG REQUIRED)
target_link_libraries(my_robot_app PRIVATE Uniubi::RobotMotionSdk)
# MediaBus applications additionally link Uniubi::MediaBus
```

Pass `-DCMAKE_PREFIX_PATH=$HOME/.local/uniubi` when configuring the consumer project. The install layout contains `include/`, `lib/<arch>/`, `lib/cmake/UniubiRobotSdk/`, and `bin/`. Programs and runtime libraries from a cross-install prefix must be deployed together to Orin and cannot run on the build host.

For source-tree development, you can continue to use the repository root as `UNIUBI_SDK_ROOT`.

### 6. Configure runtime libraries and run an example

SDK programs require root privileges on the current devices. Building does not require `sudo`. At runtime, use `sudo env` to pass the library path explicitly because `sudo` may remove the current user's `LD_LIBRARY_PATH`.

If you installed the SDK as described in section 5, run the following on the final runtime machine:

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
  "$UNIUBI_SDK_PREFIX/bin/example_highlevel" --read-only
```

To run directly from the source tree, execute in the SDK repository root:

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
  ./build/examples/example_highlevel --read-only
```

Native-build examples are under `build/examples/`; cross-build examples are under `build-aarch64/examples/`. `example_media_frames` is built by default for `aarch64`. `example_lowlevel_tensorrt` is enabled by default only for a native Orin build; a cross-build must explicitly provide the target TensorRT/CUDA development files.

### 7. Integrate without installing the SDK

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

At runtime, the dynamic linker must also be able to find matching versions and architectures of `libmediaBus.so`, `libudbus.so`, `libubase.so`, and their companion dependencies.

## Choose a Development Path

After the build and runtime library setup, choose an SDK entry point based on your goal:

| Goal | Control mode | SDK entry point | Start here |
|---|---|---|---|
| Query state, sensors, or odometry | Read-only High-level data | `MotionHighLevelClient` | `--read-only` mode in [`example_highlevel.cpp`](examples/example_highlevel.cpp) |
| Use built-in stand, lie-down, walk, and other motions | High-level | `MotionHighLevelClient` | Interactive CLI in [`example_highlevel.cpp`](examples/example_highlevel.cpp) |
| Run your own policy and directly control joint position or torque | Low-level | `MotionLowLevelClient` | [`example_lowlevel.cpp`](examples/example_lowlevel.cpp) for posture control; [`example_lowlevel_tensorrt.cpp`](examples/example_lowlevel_tensorrt.cpp) for ONNX/TensorRT policies |
| Subscribe to camera, microphone, or encoded frames | MediaBus | `MediaBusClient` | [`example_media_frames.cpp`](examples/example_media_frames.cpp) |

- **High-level:** the application sends a motion or action intent, and the robot's built-in capability closes the joint-level loop.
- **Low-level:** the application runs its own policy or controller and periodically generates joint-position or torque commands.

`queryMotionState()` reports the currently active action, not the robot posture or motor readiness. The result contains `action`, velocity, and related state fields only while an action is active. With no active action, the call still succeeds and returns the standard empty object `{}`. A connection or RPC failure returns `false`; use `getLastError()` to retrieve the cause.

If you have not selected a control mode yet, start with the [`uniubi-docs` Quick Start](https://github.com/uniubi-ai/uniubi-docs#quick-start).

## Capability and Deployment Boundaries

| Capability | On-board, single device | Remote / multiple devices | Key restriction |
|---|---:|---:|---|
| High-level control and observation | Supported | Supported | Remote mode must select a network interface and create a client with the device SN |
| Low-level joint control | Supported | Not supported | Connects directly to the local MotionServer; the data plane uses SHM |
| MediaBus frame subscription | `aarch64` only | Not supported | Depends on the on-board media service, configuration, and SHM |

The High-level and odometry examples use the on-board, single-device workflow. A remote or multi-device application must discover devices first, then create the client with the selected device SN. See the [High-level SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_high_level_sdk.md).

## Run the Examples

The following commands assume a native `build/` directory on the target machine. For a cross-build, deploy programs from `build-aarch64/examples/` to Orin and use the same `lib/aarch64/` runtime set there.

All SDK examples require root privileges on current devices. The commands use `sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH"` to preserve the SDK dynamic-library search path.

### High-level CLI: begin with read-only validation

`example_highlevel` is an interactive High-level tool. It does not start any action automatically. For the first connection, use `--read-only` so it does not request motion-control ownership:

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel --read-only
```

At the `highlevel>` prompt, begin with read-only checks:

```text
highlevel> status
highlevel> caps
highlevel> motors
highlevel> sensor 5
highlevel> odom 5
highlevel> quit
```

`odom` is part of the High-level CLI. Odometry is valid only while the robot is in the Walk action. `position` is already accumulated on the device; do not integrate it again in the application.

### High-level control validation

To control the robot, start without `--read-only`. The program obtains High-level control ownership but still does not start an action automatically:

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_highlevel
```

The default mode requests ownership after connecting but starts no action. `--read-only` only prevents ownership acquisition at startup; you can still explicitly run `take` in that session. A timed walk after explicitly taking ownership from a read-only session looks like this:

```text
highlevel> take
highlevel> start walking
highlevel> send 3 {"lineVelocityX":0.3,"lineVelocityY":0,"velocity":0}
highlevel> stop
highlevel> release
highlevel> quit
```

`start ACTION [JSON]` starts an action, `set JSON` continuously updates action parameters, and `send SECONDS JSON` sends parameters for a bounded duration and then automatically zeros walking velocity. `zero` only zeros walking velocity; it does not stop the action. `stop` stops the current action, `release` releases ownership, and `estop` requests an emergency stop. Enter `help` at any time for the current command list.

Before the first hardware run, use a clear area, keep the emergency stop within reach, and have an operator attend the robot. If the default interface is not `eth0`, pass `--iface IFACE`. For remote or multi-device mode, pass `--device-id SN`.

### Low-level joint-control validation

> **Before testing, secure the robot on a reliable safety rig with all four feet fully clear. Ensure every leg can move throughout its range without touching the floor, rig, or nearby objects. Keep the emergency stop within reach and have a dedicated operator attend the robot.**

`example_lowlevel` is an interactive Low-level tool. At startup it only connects; it neither enables motor control nor executes a posture automatically:

```bash
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  ./build/examples/example_lowlevel
```

Check state and motor layout before issuing posture commands:

```text
lowlevel> status
lowlevel> motors
lowlevel> stand
lowlevel> lie
lowlevel> damping
lowlevel> release
lowlevel> quit
```

`stand`, `lie`, and `damping` call `setMotionEnable(true)` as needed; the CLI does not wrap them in a High-level-style `take` command. `stand` and `lie` start from the live joint positions, interpolate smoothly to the target posture at 50 Hz over two seconds, and then hold it. `damping` zeros position stiffness while retaining velocity damping. `release` enters damping before disabling Low-level control. `quit` also attempts to restore the robot's built-in motion control.

The example uses the standard DV500 12-joint layout and posture parameters validated on the board. It refuses to enable posture control when the layout does not match. Keep the robot fully suspended with all feet clear throughout this Low-level example; do not run it directly on the ground. See the [Low-level SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_low_level_sdk.md) for state requirements.

### Low-level TensorRT policy validation (Jetson Orin)

A native JetPack 6.2.1 Orin build enables `example_lowlevel_tensorrt` by default. The example accepts a static `[1,45] -> [1,12]` ONNX model and rebuilds an FP32 TensorRT engine at every startup. It does not cache the engine and does not depend on PyTorch or ONNX Runtime.

Validate hardware motion in two stages. First, secure the robot on a safety rig with all four feet fully clear and validate only standing and lying down:

```text
lowlevel> stand
lowlevel> lay
lowlevel> quit
```

After confirming the postures, joint directions, and emergency stop, place the robot on clear, level, obstacle-free ground and then validate policy walking:

```text
lowlevel> stand
lowlevel> walk 0.5 0 0
lowlevel> stop
lowlevel> lay
lowlevel> quit
```

Do not execute `walk` while all four feet are suspended. During both stages, keep the emergency stop within reach and have a dedicated operator attend the robot.

```bash
cmake -S . -B build -DBUILD_SDK_TENSORRT_EXAMPLE=ON
cmake --build build --target example_lowlevel_tensorrt -j$(nproc)

# Build the engine and run one zero-input inference without initializing the SDK
taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx --validate-only

# Interactive hardware run; pinning to CPU 2 is recommended to reduce scheduler jitter
sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
  taskset -c 2 ./build/examples/example_lowlevel_tensorrt \
  --onnx /path/to/policy.onnx
```

Before enabling control, the program calls `getMotorLayout()` and verifies a 12-joint leg-major layout. It constructs control frames from the actual `limbNo` / `jointNo`. The example model uses joint-major input and output order, so the implementation explicitly performs the bidirectional `SDK leg-major -> model joint-major -> SDK leg-major` reorder. It refuses control if the layout, joint count, or model shape does not match.

On exit, the program calls `setMotionEnable(false)` only when needed, then disconnects and shuts down the SDK. It does not call `emergencyStop()` or `restoreMotionControlMode()`. See [`examples/README.md`](examples/README.md) for the complete model contract, commands, and cross-compilation dependencies.

### MediaBus validation

`example_media_frames` is only for local on-board media-frame subscription on `aarch64`. Before running it, confirm that `/etc/robot/sdk_config.json`, the media service, and the SHM environment are ready. See [Runtime Notes](docs/runtime_notes.md) for configuration and troubleshooting.

## Example Index

| Example | Acquires control | Can cause motion | Purpose |
|---|---:|---:|---|
| `example_highlevel --read-only` | No | No | Query High-level state, capabilities, sensors, and Walk odometry |
| `example_highlevel` | Yes | Only after an explicit control command | Interactive High-level CLI; keeps the control lease and tests actions and parameters step by step |
| `example_lowlevel` | Enables as required by posture/damping commands | `stand` / `lie` can | Interactive Low-level CLI for state checks, damping, and pure-position stand/lie control |
| `example_lowlevel_tensorrt` | Enables as required by `stand` / `walk` | `stand` / `walk` / `stop` / `lay` can | On-board Orin ONNX -> FP32 TensorRT Low-level policy; rebuilds at every startup |
| `example_media_frames` | No | No | Subscribe to and save on-board audio/video frames |

## Documentation

- [SDK Runtime Notes](docs/runtime_notes.md)
- [`uniubi-docs` development entry point](https://github.com/uniubi-ai/uniubi-docs)
- [Build, installation, and cross-compilation](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/BUILD.md)
- [High-level SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_high_level_sdk.md)
- [Low-level SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_low_level_sdk.md)
- [Media SDK API](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/uniubi_media_sdk.md)
- [Core Concepts](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/core-concepts/README.md)

Direct DDS / ROS 2 protocol integration is an Advanced path rather than the standard C++ SDK entry point. When needed, begin at [`uniubi-docs` Advanced](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/advanced/README.md).

## License

Original UniUbi code, headers, examples, and documentation in this repository are licensed under the Apache License 2.0. Prebuilt libraries and third-party components are licensed under their respective terms. See [LICENSE](LICENSE), [NOTICE](NOTICE), and [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
