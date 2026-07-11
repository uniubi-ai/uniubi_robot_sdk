/**
 * ========================================================
 *  @file example_highlevel.cpp
 *  @brief HighLevel 控制模式示例：连接 → 取控制权 → 动作 + 音频 → 释放 → 断开
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 * ========================================================
 */

#include <chrono>
#include <thread>
#include <cstdio>
#include <string>
#include "uniubi/robot_sdk/MotionSdkService.h"
#include "uniubi/robot_sdk/MotionHighLevelClient.h"

using namespace uniubi::RobotSdk;
using HLState = IMotionHighLevelClient::HighLevelState;
using HLError = IMotionHighLevelClient::HighLevelError;

static void onConnect(HLState state, HLError err) {
    switch (state) {
        case IMotionHighLevelClient::kControlled:
            printf("[high] control acquired\n");
            break;
        case IMotionHighLevelClient::kConnected:
            if (err == HLError::kSessionExpired)             printf("[high] lease expired\n");
            else if (err == HLError::kSessionRevoked)        printf("[high] preempted by others\n");
            else if (err == HLError::kRpcAcquireRejected)    printf("[high] startControl rejected/timeout\n");
            else                                              printf("[high] control released\n");
            break;
        default: break;
    }
}

static void onEvent(const std::string& topic, const std::string& payload) {
    if (topic == "statistics/play_list")          printf("[evt] play: %s\n", payload.c_str());
    else if (topic == "statistics/device_status") printf("[evt] dev:  %s\n", payload.c_str());
}

int main(int argc, char** argv) {

    auto svc = IMotionSdkService::instance();
    /// 远端 / 多设备场景指定网卡；板内忽略。可通过 argv[1] 覆盖默认网卡名
    svc->setNetworkInterface(argc > 1 ? argv[1] : "eth0");

    if (!svc->initialService(nullptr, "myAppHighLevel")) {
        fprintf(stderr, "SDK init failed\n");
        return 1;
    }

    /// 板内单设备直接 create(asMaster)，远端多设备先 discoverDevices 拿 SN 后 create(sn)
    /// 本示例按单设备演示；多设备流程参考接口手册 §4.1
    std::string deviceId;
    if (svc->isMultiDevice()) {
        fprintf(stderr, "multi-device mode: use discoverDevices() first to obtain a SN\n");
        svc->shutdown();
        return 1;
    }
    auto client = deviceId.empty() ? IMotionHighLevelClient::create(/*asMaster=*/false)
                                   : IMotionHighLevelClient::create(deviceId);
    if (!client) {
        fprintf(stderr, "create high level client failed\n");
        svc->shutdown();
        return 1;
    }

    /// 注册回调 —— 必须在 connect 之前
    client->setConnectCallback(&onConnect);
    client->setEventCallback(&onEvent);

    /// 1) 连接（leaseMs<=0 时 SDK 默认 60s；host proxy 按 [5s,5min] clamp）
    if (!client->connect()) {
        printf("connect failed: err=%d\n", client->getLastError());
        IMotionSdkService::instance()->shutdown();
        return 1;
    }

    /// 2) 查询类接口：能力 + 系统状态
    std::string caps;
    if (client->getMotionCapabilities(caps)) {
        printf("capabilities: %s\n", caps.c_str());
    }

    std::string status;
    if (client->querySystemStatus(status)) {
        printf("system: %s\n", status.c_str());
    }

    /// 3) 取控制权（异步；timeout 是整体截止时间）
    if (!client->startControl(/*timeout=*/30000)) {
        printf("startControl failed: err=%d\n", client->getLastError());
        client->disconnect();
        IMotionSdkService::instance()->shutdown();
        return 1;
    }

    /// 等回调切到 kControlled —— 实际应用建议在 onConnect 里推进，这里用有限时间轮询
    const auto controlDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (client->getState() != IMotionHighLevelClient::kControlled) {
        if (std::chrono::steady_clock::now() >= controlDeadline) {
            printf("wait kControlled timeout: state=%d err=%d\n",
                   client->getState(), client->getLastError());
            client->disconnect();
            IMotionSdkService::instance()->shutdown();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /// 4) 首跑安全动作
    client->standUp();
    std::this_thread::sleep_for(std::chrono::seconds(5));
    client->lieDown();
    std::this_thread::sleep_for(std::chrono::seconds(5));

    /// 4b) 原始 TRC 控制帧（DDS topic motion/trc，不走 RPC）
    ///     默认不发送真实原始控制帧；需要动作调试时打开开关，并确认场地、急停和人工接管。
    ///     前置：host proxy 已 setup mTRCReader 并在 acquire 响应里下发 rawActionId（SDK 持权后自动接收）
    ///     若服务端 TRC reader 未配置 → SDK 不到 rawActionId → setRawControlCmd 返 false + kActionRejected
    constexpr bool kEnableRawControlDemo = false;
    if (kEnableRawControlDemo) {
        TRCStickFrame frame = {};
        frame.valid = 1;
        frame.buttons[buttonBack] = 1;        /// Stand
        frame.buttons[buttonA]    = 1;        /// Stand + A -> Lie Down（内部动作 laying）
        frame.axes[axesLX]      = 0.0f;
        frame.axes[axesLY]      = 0.0f;
        frame.axes[axesRX]      = 0.0f;
        frame.axes[axesRY]      = 0.0f;
        if (!client->setRawControlCmd(frame)) {
            printf("setRawControlCmd skipped/failed: err=%d\n", client->getLastError());
        }
    }

    /// 5) 音频播放（启动 → 暂停 → 停止），音频接口直接挂在 client 上
    client->startAudioPlay(R"({"list":[{"id":"1"}],"volume":50,"repeat":1})");
    std::this_thread::sleep_for(std::chrono::seconds(5));
    client->pauseAudioPlay();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    client->stopAudioPlay();

    /// 6) 查询：音频详情 + 文件列表
    std::string audioDetail;
    if (client->queryAudioPlayDetail(audioDetail)) {
        printf("audio detail: %s\n", audioDetail.c_str());
    }

    std::string audioList;
    if (client->queryAudioPlayList(audioList, R"({"type":"customVoice"})")) {
        printf("audio list: %s\n", audioList.c_str());
    }

    /// 7) 观测量上报：开启后运控帧经 setMotionObservedCallback 回调上抛，GPS 经 setGPSCallback；
    ///    getPowerInfo 的 timeout 是 us 级新鲜度窗口（仅返回此窗口内的最新电源量）
    client->setMotionObservedCallback([](const LowLevelMotionObserved& obs) {
        printf("[obs] motorNum=%u imuTemp=%.1f power=%.1f%%\n",
               obs.motorNum, obs.imu.temp, obs.power.power);
    });
    client->setGPSCallback([](const GPSFrame& gps) {
        printf("[gps] valid=%u point=(%.6f,%.6f) speed=%.2f\n",
               gps.valid, gps.point.lat, gps.point.lng, gps.speed);
    });
    std::string observedState;
    client->setObservedEnable(R"({"motionEnable":true,"sensorEnable":true})", observedState);
    printf("[obs] enabled, state=%s\n", observedState.c_str());
    std::this_thread::sleep_for(std::chrono::seconds(5));

    PowerObserved power = {};
    if (client->getPowerInfo(&power, 1000000)) {
        printf("[power] level=%.1f%% health=%.1f temper=%.1f charge=%.2fA/%.2fV\n",
               power.power, power.health, power.temper, power.chargeCurrent, power.chargeVoltage);
    }
    client->setObservedEnable(R"({"motionEnable":false,"sensorEnable":false})", observedState);

    /// 8) 退出 —— 显式释放 + 显式 disconnect + shutdown
    client->releaseControl();
    client->disconnect();
    IMotionSdkService::instance()->shutdown();
    return 0;
}
