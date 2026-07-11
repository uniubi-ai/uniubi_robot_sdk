/**
 * ========================================================
 *  @file example_lowlevel.cpp
 *  @brief LowLevel 控制模式示例：回调驱动 + 异步 setMotionEnable
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 * ========================================================
 */

#include <mutex>
#include <chrono>
#include <thread>
#include <cstdio>
#include <cstring>
#include <condition_variable>
#include "uniubi/robot_sdk/MotionSdkService.h"
#include "uniubi/robot_sdk/MotionLowLevelClient.h"

using namespace uniubi::RobotSdk;
using LLState = IMotionLowLevelClient::LowLevelState;
using LLError = IMotionLowLevelClient::LowLevelError;

int main() {

    auto svc = IMotionSdkService::instance();

    if (!svc->initialService(nullptr, "myAppLowLevel")) {
        fprintf(stderr, "SDK init failed\n");
        return 1;
    }

    /// 低级控制为板内单设备部署，直接 create()（进程内单例）
    auto client = IMotionLowLevelClient::create();
    if (!client) {
        fprintf(stderr, "create low level client failed\n");
        svc->shutdown();
        return 1;
    }

    /// 回调线程与主线程间用 cv 同步 state 变化通知
    std::mutex stateMu;
    std::condition_variable stateCv;

    client->setConnectCallback([&](LLState state, LLError err) {
        switch (state) {
            case LLState::kPrepared:
                printf("[low] prepared (ready for sendControl)\n"); break;
            case LLState::kConnected:
                printf("[low] connected (call setMotionEnable to prepare)\n"); break;
            case LLState::kConnecting:
                printf("[low] connecting: err=%d\n", static_cast<int>(err)); break;
            case LLState::kConnectionLost:
                printf("[low] connection lost (auto-reconnecting): err=%d\n", static_cast<int>(err)); break;
            case LLState::kDisconnected:
                printf("[low] disconnected: err=%d\n", static_cast<int>(err)); break;
        }
        if (err == LLError::kMasterSwitchFailed) {
            printf("[low] master role switch failed (peer may hold motion), retrying...\n");
        }
        std::lock_guard<std::mutex> lk(stateMu);
        stateCv.notify_all();
    });

    /// 等到 state 命中 target 或超时；用 cv 等回调通知
    auto waitStateCb = [&](LLState target, int timeoutMs) -> bool {
        std::unique_lock<std::mutex> lk(stateMu);
        return stateCv.wait_for(lk, std::chrono::milliseconds(timeoutMs),
            [&] { return client->getState() == static_cast<int32_t>(target); });
    };

    /// connect(observedHz, leaseMs)：leaseMs=0 → server 默认；server 按自身策略 clamp 后下发真实值
    if (!client->connect(/*observedHz=*/500, /*leaseMs=*/60000)) {
        printf("connect failed: %d\n", client->getLastError());
        IMotionSdkService::instance()->shutdown();
        return 1;
    }

    /// 等到 kConnected 才能切 prepare
    if (!waitStateCb(LLState::kConnected, 10000)) {
        printf("wait connected timeout\n");
        client->disconnect();
        IMotionSdkService::instance()->shutdown();
        return 1;
    }

    /// 异步请求开启 motion；返回 true 仅表示请求已受理，state 由 SDK 推进至 kPrepared
    if (!client->setMotionEnable(true)) {
        printf("setMotionEnable(true) request rejected: %d\n", client->getLastError());
        client->disconnect();
        IMotionSdkService::instance()->shutdown();
        return 1;
    }

    /// 等回调把 state 推进到 kPrepared
    /// motor enable 是异步过程，每个关节上电 + 校验需要几百 ms 到几秒，留充足超时
    if (!waitStateCb(LLState::kPrepared, 60000)) {
        printf("wait prepared timeout\n");
        client->disconnect();
        IMotionSdkService::instance()->shutdown();
        return 1;
    }

    /// 按真实硬件布局构造零力矩控制模板：从 getMotorLayout 拿到电机数 + 每电机 (limbNo, jointNo)
    MotorLayout layout = {};
    if (!client->getMotorLayout(layout)) {
        printf("getMotorLayout failed: %d\n", client->getLastError());
        client->setMotionEnable(false);
        client->disconnect();
        IMotionSdkService::instance()->shutdown();
        return 1;
    }
    printf("[low] motor layout: %u motor(s)\n", layout.motorNum);
    for (uint32_t i = 0; i < layout.motorNum; ++i) {
        printf("  motor[%u]: limb=%u joint=%u name=%s\n",
               i, layout.motors[i].limbNo, layout.motors[i].jointNo, layout.motors[i].name);
    }

    /// 硬件首跑安全前提：
    /// - 仅在吊架 / 急停可触达 / 空旷场地条件下运行；
    /// - 下方零目标、零增益、零前馈力矩只是通信与观测闭环模板，不是平衡站立控制器；
    /// - 真实闭环控制应从当前观测姿态初始化目标，并使用经过验证的阻尼、增益和力矩策略。
    MotorCtrlAction action = {};
    action.motorNum = layout.motorNum;
    for (uint32_t i = 0; i < layout.motorNum; ++i) {
        auto& m = action.motors[i];
        m.position = 0.0f;m.velocity = 0.0f;
        m.kpGain = 0.0f;m.kdGain = 0.0f;m.torque = 0.0f;
        m.header.limbNo  = layout.motors[i].limbNo;
        m.header.jointNo = layout.motors[i].jointNo;
    }

    constexpr int32_t kStandingAction = 1;
    LowLevelMotionCmd cmd = {};
    cmd.action = kStandingAction;
    snprintf(cmd.acName, sizeof(cmd.acName), "%s", "standing");

    /// 首次联调建议 50Hz × 60s；长稳态验证可在安全闭环确认后按需要延长
    /// getLatestObservation 内部已合并 publish 请求 + 等 server 写 + 读 SHM
    constexpr uint32_t kObsTimeoutMs = 5;                             /// SDK 总等待 5ms（含 server 处理）
    constexpr auto kCyclePeriod = std::chrono::milliseconds(20);      /// 50Hz

    LowLevelMotionObserved obs = {};
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(60);
    auto nextCycle = std::chrono::steady_clock::now();
    int obsFailCount = 0;
    auto obsLastLogAt = std::chrono::steady_clock::now();
    auto dumpAt = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() < deadline) {
        nextCycle += kCyclePeriod;

        if (!client->getLatestObservation(&obs, kObsTimeoutMs)) {
            ++obsFailCount;
            auto now = std::chrono::steady_clock::now();
            if (now - obsLastLogAt >= std::chrono::seconds(1)) {
                printf("getLatestObservation failed %d times/1s, state=%d lastErr=%d\n",
                       obsFailCount, client->getState(), client->getLastError());
                obsFailCount = 0;
                obsLastLogAt = now;
            }
            std::this_thread::sleep_until(nextCycle);
            continue;
        }

        /// 1Hz 打印观测量摘要（IMU / power / 第 0 号电机）
        auto now = std::chrono::steady_clock::now();
        if (now - dumpAt >= std::chrono::seconds(1)) {
            const auto& a = obs.imu.accel; const auto& g = obs.imu.gyro; const auto& q = obs.imu.quaternion;
            const auto& m0 = obs.motors[0];
            printf("[obs] imu: temp=%.1f  accel=(%+.2f,%+.2f,%+.2f)  gyro=(%+.3f,%+.3f,%+.3f)  "
                   "quat=(%.3f,%.3f,%.3f,%.3f)  power=%.2fV  motor[0]: pos=%+.3f vel=%+.3f torq=%+.3f\n",
                   obs.imu.temp, a.x, a.y, a.z, g.x, g.y, g.z, q.w, q.x, q.y, q.z,
                   obs.power.chargeVoltage, m0.position, m0.velocity, m0.torque);
            dumpAt = now;
        }

        if (!client->sendControl(action, &cmd)) {
            printf("send_control failed: %d\n", client->getLastError());
            break;
        }
        std::this_thread::sleep_until(nextCycle);
    }

    /// 额外：拉一次传感器观测（gps/uwb），低频即可；timeout 为 us 级新鲜度窗口
    SensorObserved sensor = {};
    if (client->getSensorObservation(&sensor, 1000000)) {
        const auto& gps = sensor.gps;
        const auto& uwb = sensor.uwb;
        printf("[sensor] gps valid=%u point=(%.6f,%.6f) speed=%.2f  uwb valid=%u dist=%u azimuth=%u\n",
               gps.valid, gps.point.lat, gps.point.lng, gps.speed,
               static_cast<unsigned>(uwb.valid), uwb.distance, static_cast<unsigned>(uwb.azimuth));
    }

    client->setMotionEnable(false);
    client->disconnect();
    IMotionSdkService::instance()->shutdown();
    return 0;
}
