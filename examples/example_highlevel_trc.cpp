// Read-only High-level TRC input. No control acquisition or motion commands.
#include "uniubi/robot_sdk/MotionHighLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <csignal>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

using namespace uniubi::RobotSdk;
namespace {
volatile std::sig_atomic_t stopping = 0;
void stop(int) { stopping = 1; }
const char* buttons[] = {"Back", "Start", "LB", "RB", "F1", "F2", "A", "B",
                        "X", "Y", "Up", "Down", "Left", "Right", "LS", "RS"};
const char* axes[] = {"LX", "LY", "RX", "RY", "LT", "RT"};
struct Samples {
    std::mutex mutex;
    unsigned count = 0;
    bool seen = false;
    TRCStickFrame previous{};
};
}
int main(int argc, char** argv) {
    if (argc > 4 || (argc > 1 && std::string(argv[1]) == "--help")) {
        std::cout << "Usage: " << argv[0] << " [IFACE] [DEVICE_ID|-] [SECONDS]\n"
                  << "Defaults: eth0.100, board-local device, 60 seconds.\n";
        return argc > 4 ? 1 : 0;
    }
    int seconds = 60;
    try {
        if (argc > 3) {
            std::size_t used = 0;
            seconds = std::stoi(argv[3], &used);
            if (used != std::string(argv[3]).size() || seconds <= 0)
                throw std::runtime_error("SECONDS must be positive");
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
    const std::string device = argc > 2 && std::string(argv[2]) != "-" ? argv[2] : "";
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    auto service = IMotionSdkService::instance();
    service->setNetworkInterface(argc > 1 ? argv[1] : "eth0.100");
    if (!service->initialService(nullptr, "highlevel-trc-example")) return 1;
    std::shared_ptr<IMotionHighLevelClient> client;
    auto samples = std::make_shared<Samples>();
    bool attempted = false;
    int status = 0;
    try {
        if (service->isMultiDevice() && device.empty())
            throw std::runtime_error("External hosts require DEVICE_ID (robot SN)");
        client = device.empty() ? IMotionHighLevelClient::create(false)
                                : IMotionHighLevelClient::create(device);
        if (!client) throw std::runtime_error("client creation failed");
        client->setMotionObservedCallback([samples](const LowLevelMotionObserved& obs) {
            std::lock_guard<std::mutex> lock(samples->mutex);
            ++samples->count;
            const auto& t = obs.trc;
            auto& old = samples->previous;
            if (!t.valid) {
                if (!samples->seen || old.valid) std::cout << "TRC invalid\n";
                old = t; samples->seen = true; return;
            }
            if (!samples->seen || !old.valid) {
                std::cout << "TRC baseline:";
                for (unsigned i = 0; i < BUTTON_MAX; ++i)
                    std::cout << ' ' << buttons[i] << '=' << bool(t.buttons[i]);
                for (unsigned i = 0; i < AXES_MAX; ++i)
                    std::cout << ' ' << axes[i] << '=' << t.axes[i];
                std::cout << std::endl;
                old = t; samples->seen = true; return;
            }
            for (unsigned i = 0; i < BUTTON_MAX; ++i) {
                if (bool(t.buttons[i]) != bool(old.buttons[i]))
                    std::cout << buttons[i] << (t.buttons[i] ? " pressed" : " released") << std::endl;
                old.buttons[i] = t.buttons[i];
            }
            for (unsigned i = 0; i < AXES_MAX; ++i)
                if (std::fabs(t.axes[i] - old.axes[i]) >= 0.02f) {
                    std::cout << axes[i] << ' ' << t.axes[i] << std::endl;
                    old.axes[i] = t.axes[i];
                }
        });
        if (!client->connect(60000)) throw std::runtime_error("connect failed");
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        std::string result;
        while (!stopping && !client->getMotionCapabilities(result)) {
            if (std::chrono::steady_clock::now() >= deadline)
                throw std::runtime_error("device discovery timed out");
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        if (!stopping) {
            attempted = true;
            if (!client->setObservedEnable(R"({"motionEnable":true,"trcEnable":true})", result))
                throw std::runtime_error("enable observations failed");
            std::cout << "Ready: observing for " << seconds << " seconds; Ctrl+C to exit" << std::endl;
            deadline = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
            while (!stopping && std::chrono::steady_clock::now() < deadline)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; status = 1; }
    if (client) {
        std::string result;
        if (attempted && !client->setObservedEnable(
                R"({"motionEnable":false,"trcEnable":false})", result)) {
            std::cerr << "Failed to disable observations\n"; status = 1;
        }
        client->disconnect();
    }
    service->shutdown();
    std::cout << "Observation callbacks: " << samples->count << '\n';
    if (attempted && samples->count == 0) {
        std::cerr << "No observations received\n"; status = 1;
    }
    return status;
}
