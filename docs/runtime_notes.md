# Runtime Notes

[中文文档](runtime_notes.zh-CN.md)

This document records runtime behaviors that commonly cause integration mistakes in the C++ SDK. Complete interface documentation is maintained in [uniubi-docs](https://github.com/uniubi-ai/uniubi-docs).

## High-level Actions Are Asynchronous

A successful return from `startAction()`, `standUp()`, or `lieDown()` only means that the robot accepted the request. It does not mean the physical posture has already reached its target.

`queryMotionState()` reports only the currently active action. The result contains `action`, velocity, and related fields only while an action is active. With no active action, the RPC still succeeds and returns the empty object `{}`. An empty object does not indicate an abnormal posture or motors that are not ready. An RPC or connection failure returns `false`; use `getLastError()` to identify the error.

Use an observation-based closed loop when finishing a test or exiting an application:

1. Call `stopAction()`.
2. Call `lieDown()` or `startAction("laying")`.
3. Poll `queryMotionState()` until it returns an empty object (`{}`) or an object containing `"action":"laying"`.
4. Then call `releaseControl()`, `disconnect()`, and `IMotionSdkService::shutdown()`.

Do not release the connection merely because an RPC succeeded while the robot may still be executing `walking` or another action.

## Adding an Audio URL Is Asynchronous

`addAudioFile()` may only indicate that a download task was accepted. The target audio appears in the custom audio list only after the robot has downloaded and stored it.

Recommended workflow:

1. Call `addAudioFile()` with a stable `id` and URL.
2. Poll `queryAudioPlayList(out, R"({"type":"customVoice"})")`.
3. Play the audio only after the target `id` appears.
4. Stop playback before deleting it.

## Verify the Observation Loop Before Restoring Built-in Motion Control

`restoreMotionControlMode()` explicitly returns motion-control ownership to the robot's default built-in controller when an application needs that transition. It is not a mandatory step before every `disconnect()`, and `disconnect()` does not perform this transition automatically.

The C++ control interface is `sendControl(action, cmd = nullptr)`. For action-related frames, pass a `LowLevelMotionCmd` and populate both the action ID and action name. For example, standing uses `action = 1` and `acName = "standing"`. This helps both internal server interpretation and external observation.

A `true` result from `sendControl()` only means that the frame was submitted. If the application decides to restore built-in motion control, first use observations to confirm that the robot has reached a safe posture:

1. Control the robot into the expected safe posture, normally laying.
2. Continue calling `getLatestObservation()` until joint positions are close to the target posture.
3. Call `setMotionEnable(false)` and wait for the state to return to `kConnected`.
4. Call `restoreMotionControlMode()` and check its return value.

Skipping the observation check can hand control back while the robot is still transitioning.

## Low-level Maximum Torque Is a Low-frequency Configuration

`sendMaxTorque(action)` takes effect only in the `kPrepared` state. `action.motorNum` must be within `[1, kLowLevelMaxMotorNum]`. Each element identifies a motor with `header.limbNo` / `header.jointNo` and specifies the target maximum torque in N·m with `torque`. Build the complete configuration from the layout returned by `getMotorLayout()`.

A `true` return only means that the configuration frame was submitted to shared memory; it does not mean the motor-side transition has completed. The lower layer has a default torque-switching window of approximately 10 ms during which position commands are unsupported. Do not put this interface in a high-frequency `sendControl()` loop or continue sending position frames during the switching window. Confirm the observed value later through `motors[i].maxTorque` from `getLatestObservation()`.

## Local MediaBus Configuration

`IMediaBusClient` provides local, on-board media-frame subscription on `aarch64`. Remote or multi-device SDK mode does not provide MediaBus frame subscription. On `x86_64` / `i386`, do not call media-client interfaces such as `createMediaBusClient()`, `setup()`, or `start*Frame()`.

For on-board deployment, `LocalMediaBusClient` always reads `/etc/robot/sdk_config.json`, which must contain a top-level `streamDefine` object. `setup()` fails when the file is missing or malformed:

| Error | Common cause |
|---|---|
| `kConfigLoadFailed` | `/etc/robot/sdk_config.json` is missing or unreadable |
| `kConfigInvalid` | The file exists but has no top-level `streamDefine` object |
| `kMediaInitFailed` / `kMediaStartFailed` | The media service, stream channel, runtime libraries, or SHM environment is not ready |

Minimal on-board configuration:

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

Notes:

- The length of `viStream` becomes `MediaLayout::cameraNum`.
- The length of `aiStream` becomes `MediaLayout::micNum`.
- `streamSource.localChannel` becomes `MediaLayout::videoEncoderNum`.
- This JSON is not a Cyclone DDS XML configuration.
- Successful `setup()` and `getMediaLayout()` calls only confirm initialization and capability discovery. To verify media availability, subscribe and count frames continuously for several seconds.

## Runtime Library and SHM Checks

Runtime libraries must be a matched set from the same delivery version and target architecture. `librobotMotionSdk.so` and `libmediaBus.so` directly depend on `libudbus.so` and `libubase.so`; do not mix versions of these four files. DDS and iceoryx libraries must also match the delivery, otherwise failures can appear as service timeouts, initialization errors, or subscriptions that receive no frames.

SDK programs require root privileges on current devices. On-board Low-level and MediaBus paths also depend on a restricted shared-memory environment. Start programs with `sudo env LD_LIBRARY_PATH="$LD_LIBRARY_PATH" ...` as described in the README. Do not work around this requirement by relaxing system-file or SHM permissions.
