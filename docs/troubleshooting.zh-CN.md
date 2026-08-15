# 故障排查

[English](troubleshooting.md)

本文汇总 C++ SDK 接入中的常见问题、原因及检查或处理方法。完整接口说明见 C++ API 文档：[High-level](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/api-reference/cpp/high-level.zh-CN.md)、[Low-level](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/api-reference/cpp/low-level.zh-CN.md) 和 [MediaBus](https://github.com/uniubi-ai/uniubi-docs/blob/main/docs/api-reference/cpp/media.zh-CN.md)。

## High-level 动作是异步的

`startAction()`、`standUp()`、`lieDown()` 返回成功，只代表机器人已接受请求，不代表真实姿态已经到位。

`queryMotionState()` 只报告当前活动 action。只有存在活动 action 时，返回对象才包含 `action`、速度等字段；没有活动 action 时 RPC 仍然成功，并返回空对象 `{}`。空对象不表示机器人姿态异常，也不表示电机未就绪。RPC 或连接失败时接口返回 `false`，应通过 `getLastError()` 判断错误。

测试收尾或业务退出时，建议使用观测闭环：

1. 调用 `stopAction()`。
2. 调用 `lieDown()` 或 `startAction("laying")`。
3. 轮询 `queryMotionState()`，直到返回空对象（`{}`）或包含 `"action":"laying"`。
4. 再调用 `releaseControl()`、`disconnect()` 和 `IMotionSdkService::shutdown()`。

不要在机器人仍可能处于 `walking` 或其它动作执行中时，只因为 RPC 返回成功就释放连接。

## 音频 URL 入库是异步的

`addAudioFile()` 可能只表示下载任务已被受理。机器人下载并保存完成后，目标音频才会出现在自定义音频列表中。

推荐流程：

1. 使用稳定的 `id` 和 URL 调用 `addAudioFile()`。
2. 轮询 `queryAudioPlayList(out, R"({"type":"customVoice"})")`。
3. 只在目标 `id` 出现后再播放。
4. 删除前先停止播放。

## 按需恢复小脑运控前要确认观测闭环

`restoreMotionControlMode()` 用于在开发者需要恢复机器人内置运控能力时，显式将运控主控切回默认的小脑模式。该接口不是每次 `disconnect()` 前的强制步骤；`disconnect()` 也不会自动执行这次切换。

C++ 控制接口为 `sendControl(action, cmd = nullptr)`。动作相关控制帧建议传入 `LowLevelMotionCmd`，并同时填写动作 id 和动作名，例如站立使用 `action = 1`、`acName = "standing"`，便于服务端内部理解和外部观测。

`sendControl()` 返回 `true` 只代表控制帧已提交。如果应用决定恢复小脑运控，需要先通过观测确认机器人已经到达安全姿态：

1. 将机器人控制到预期安全姿态，通常是 laying。
2. 持续调用 `getLatestObservation()`，确认关节位置接近目标姿态。
3. 调用 `setMotionEnable(false)`，等待状态回到 `kConnected`。
4. 调用 `restoreMotionControlMode()` 并检查返回值。

跳过观测检查，可能会在机器人仍处于过渡姿态时交回控制权。

## Low-level 最大扭矩设置是低频配置

`sendMaxTorque(action)` 仅在 `kPrepared` 状态下生效。`action.motorNum` 必须在 `[1, kLowLevelMaxMotorNum]` 范围内；每个元素使用 `header.limbNo` / `header.jointNo` 定位电机，并使用 `torque` 表示目标最大扭矩（N·m）。建议基于 `getMotorLayout()` 返回的布局构造完整配置。

返回 `true` 只代表配置帧已提交到共享内存，不代表电机侧已经完成切换。底层默认存在约 10 ms 的扭矩切换窗口，期间不支持位置控制指令；不要将该接口放入高频 `sendControl()` 循环，也不要在切换窗口内继续下发位置控制帧。可通过后续 `getLatestObservation()` 返回的 `motors[i].maxTorque` 确认当前观测值。

## MediaBus 本地配置

`IMediaBusClient` 用于 `aarch64` 板内本地媒体帧订阅。远端 / 多设备 SDK 模式不提供 MediaBus 帧订阅；`x86_64` / `i386` 平台不要调用 `createMediaBusClient()`、`setup()` 或 `start*Frame()` 等 media client 接口。

板内部署时，`LocalMediaBusClient` 固定读取 `/etc/robot/sdk_config.json`，并要求存在顶层 `streamDefine` 对象。配置缺失或格式错误时，`setup()` 会失败：

| 错误 | 常见原因 |
|---|---|
| `kConfigLoadFailed` | `/etc/robot/sdk_config.json` 缺失或不可读 |
| `kConfigInvalid` | 文件存在，但没有顶层 `streamDefine` 对象 |
| `kMediaInitFailed` / `kMediaStartFailed` | 媒体服务、流通道、运行库或 SHM 运行环境未就绪 |

板内最小配置示例：

```json
{
  "streamDefine": {
    "streamMemory": {
      "total": 5,
      "unit": "M",
      "chunk": 1024,
      "align": 4
    },
    "mediaBus": {
      "domain": "mediaBus",
      "node": "sdkClient",
      "server": "mediaServer",
      "memoryPool": []
    },
    "viStream": [
      {
        "streamNo": 0,
        "channel": {
          "name": "mediaServer.viChannel.0"
        }
      }
    ],
    "aiStream": [
      {
        "streamNo": 0,
        "channel": {
          "name": "mediaServer.aiChannel.0.0"
        }
      }
    ],
    "videoEncode": [
      [
        {
          "stream": 0,
          "encoder": 0,
          "viDevice": 0,
          "codec": 1
        }
      ]
    ],
    "audioEncode": [
      {
        "encoder": 0,
        "aiDevice": 0
      }
    ],
    "streamSource": {
      "localChannel": 1,
      "attribute": [
        {
          "stream": 1,
          "attachVideo": 0,
          "attachAudio": 0
        }
      ]
    }
  }
}
```

说明：

- `viStream` 数组长度会成为 `MediaLayout::cameraNum`。
- `aiStream` 数组长度会成为 `MediaLayout::micNum`。
- `streamSource.localChannel` 会成为 `MediaLayout::videoEncoderNum`。
- 这个 JSON 不是 Cyclone DDS XML 配置。
- `setup()` 和 `getMediaLayout()` 成功只代表初始化和能力查询成功。要确认媒体可用，应订阅并持续统计数秒帧数。

## 运行库与 SHM 检查

运行库必须使用同一交付版本、同一目标架构的一组文件。`librobotMotionSdk.so` 和 `libmediaBus.so` 直接依赖 `libudbus.so` 与 `libubase.so`，四者不能跨版本混用；DDS 库和 iceoryx 库也必须与交付包匹配，否则可能表现为服务超时、初始化失败或订阅无帧。

当前设备运行 SDK 程序需要 root 权限；板内 Low-level 和 MediaBus 链路还依赖受限的共享内存环境。应按 README 使用 `sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" ...` 启动，不要通过放宽系统文件或 SHM 权限来绕过要求。
