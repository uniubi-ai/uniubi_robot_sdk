/**
 * @file example_odometry.cpp
 * @brief 从统一传感器观测读取 Walk 平面里程计；不申请控制权、不下发动作。
 */

#include <chrono>
#include <cstdio>
#include <thread>

#include "uniubi/robot_sdk/MotionHighLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"

using namespace uniubi::RobotSdk;

static void printOdometry(const char* source, const MotionOdometry& odom) {
    // position 已由设备端累计，上层不要再次积分。
    std::printf("[%s] epoch=%u valid=%u "
                "position=(%.3f, %.3f) m velocity=(%.3f, %.3f) m/s "
                "yaw=%.3f rad yawSpeed=%.3f rad/s\n",
                source,
                odom.epoch,
                static_cast<unsigned>(odom.valid),
                odom.position[0], odom.position[1],
                odom.velocity[0], odom.velocity[1],
                odom.yaw, odom.yawSpeed);
}

int main(int argc, char** argv) {
    auto service = IMotionSdkService::instance();
    service->setNetworkInterface(argc > 1 ? argv[1] : "eth0");
    if (!service->initialService(nullptr, "myAppOdometry")) {
        std::fprintf(stderr, "SDK init failed\n");
        return 1;
    }

    auto client = IMotionHighLevelClient::create(/*asMaster=*/false);
    if (!client) {
        std::fprintf(stderr, "create high level client failed\n");
        service->shutdown();
        return 1;
    }

    // 传感器回调必须在 connect() 前注册；订阅本身不需要 High Level 控制权。
    client->setSensorObservedCallback([](const SensorObserved& sensor) {
        if (sensor.odom.valid) {
            printOdometry("callback", sensor.odom);
        }
    });

    if (!client->connect()) {
        std::fprintf(stderr, "connect failed: err=%d\n", client->getLastError());
        service->shutdown();
        return 1;
    }

    // 等待数据面更新，再读取 1000 ms 新鲜度窗口内的 SDK 缓存。
    std::this_thread::sleep_for(std::chrono::seconds(3));
    SensorObserved latest = {};
    if (client->getSensorObservation(&latest, /*timeout=*/1000)) {
        printOdometry("latest", latest.odom);
    } else {
        std::fprintf(stderr, "no fresh odometry: err=%d\n", client->getLastError());
    }

    client->disconnect();
    service->shutdown();
    return 0;
}
