/**
 * @file example_highlevel.cpp
 * @brief Interactive High-level SDK CLI. No motion action is started automatically.
 */

#include <atomic>
#include <chrono>
#include <csignal>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

#include "uniubi/robot_sdk/MotionHighLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"

namespace {

using namespace uniubi::RobotSdk;
using Client = IMotionHighLevelClient;
using ClientPtr = std::shared_ptr<Client>;

constexpr const char* kStopVelocity =
    R"({"lineVelocityX":0.0,"lineVelocityY":0.0,"velocity":0.0})";

volatile std::sig_atomic_t gStopping = 0;

void onSignal(int) {
    gStopping = 1;
}

std::string trimLeft(std::string value) {
    const auto first = value.find_first_not_of(" \t");
    return first == std::string::npos ? std::string() : value.substr(first);
}

std::string rest(std::istringstream& input) {
    std::string value;
    std::getline(input, value);
    return trimLeft(value);
}

struct Options {
    std::string iface = "eth0";
    std::string clientId = "uniubi-highlevel-cli";
    std::string deviceId;
    int32_t leaseMs = 15000;
    bool readOnly = false;
    bool discoverOnly = false;
    bool ifaceProvided = false;
};

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n\n"
        << "Options:\n"
        << "  --iface IFACE       DDS interface; required for discovery/remote mode\n"
        << "  --client-id ID      SDK client id\n"
        << "  --device-id SN      target SN for external-host device addressing\n"
        << "  --lease-ms MS       control lease hint (default: 15000)\n"
        << "  --read-only         connect without acquiring High-level control\n"
        << "  --discover-only     list discovered devices, then exit without connecting\n"
        << "  -h, --help          show this help\n\n"
        << "The CLI never starts an action automatically. Type 'help' after connecting.\n";
}

bool parseInt32(const char* text, int32_t& value) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (!text[0] || !end || *end != '\0' || parsed <= 0 || parsed > 2147483647L) {
        return false;
    }
    value = static_cast<int32_t>(parsed);
    return true;
}

bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }
        if (arg == "--read-only") {
            options.readOnly = true;
            continue;
        }
        if (arg == "--discover-only") {
            options.discoverOnly = true;
            continue;
        }
        if (arg == "--iface" || arg == "--client-id" || arg == "--device-id" ||
            arg == "--lease-ms") {
            if (++i >= argc) {
                std::cerr << "missing value for " << arg << '\n';
                return false;
            }
            if (arg == "--iface") {
                options.iface = argv[i];
                options.ifaceProvided = true;
            }
            else if (arg == "--client-id") options.clientId = argv[i];
            else if (arg == "--device-id") options.deviceId = argv[i];
            else if (!parseInt32(argv[i], options.leaseMs)) {
                std::cerr << "invalid --lease-ms: " << argv[i] << '\n';
                return false;
            }
            continue;
        }
        std::cerr << "unknown option: " << arg << '\n';
        printUsage(argv[0]);
        return false;
    }
    if ((options.discoverOnly || !options.deviceId.empty()) &&
        !options.ifaceProvided) {
        std::cerr << "--discover-only and remote --device-id require an explicit --iface\n";
        return false;
    }
    if (options.discoverOnly && !options.deviceId.empty()) {
        std::cerr << "--discover-only does not accept --device-id; it never selects a device\n";
        return false;
    }
    return true;
}

class DiscoveryResults {
public:
    void add(const std::string& sn, const std::string& info) {
        if (sn.empty()) return;
        {
            std::lock_guard<std::mutex> lock(mutex_);
            devices_[sn] = info;
        }
        changed_.notify_all();
    }

    void waitFor(std::chrono::seconds duration) {
        const auto deadline = std::chrono::steady_clock::now() + duration;
        std::unique_lock<std::mutex> lock(mutex_);
        while (!gStopping && std::chrono::steady_clock::now() < deadline) {
            changed_.wait_until(lock, deadline);
        }
    }

    bool empty() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return devices_.empty();
    }

    void print() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::cout << "Discovered " << devices_.size() << " unique device(s):\n";
        for (const auto& device : devices_) {
            std::cout << "  SN=" << device.first << " info=" << device.second << '\n';
        }
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable changed_;
    std::map<std::string, std::string> devices_;
};

bool runDiscovery(IMotionSdkService* service, const std::shared_ptr<DiscoveryResults>& results) {
    constexpr uint32_t kDiscoveryWindowMs = 5000;
    if (!service->discoverDevices(kDiscoveryWindowMs)) {
        std::cerr << "[FAIL] device discovery request failed\n";
        return false;
    }
    results->waitFor(std::chrono::seconds(5));

    if (results->empty() && !gStopping) {
        std::cout << "[INFO] no discovery callback in 5s; retrying once\n";
        if (!service->discoverDevices(kDiscoveryWindowMs)) {
            std::cerr << "[FAIL] device discovery retry failed\n";
            return false;
        }
        results->waitFor(std::chrono::seconds(5));
    }

    results->print();
    if (results->empty()) {
        std::cerr << "[FAIL] no device discovered; verify --iface and network reachability\n";
        return false;
    }
    std::cout << "[INFO] discovery is read-only; rerun with an explicit --device-id SN\n";
    return true;
}

class HighLevelCli {
public:
    explicit HighLevelCli(ClientPtr client) : client_(std::move(client)) {
        client_->setConnectCallback([](Client::HighLevelState state, Client::HighLevelError error) {
            if (state == Client::kControlled) {
                std::cout << "\n[INFO] High-level control acquired\n";
            } else if (state == Client::kConnected && error != Client::kNone) {
                std::cout << "\n[WARN] control lost, error=" << static_cast<int>(error) << '\n';
            }
        });
        client_->setEventCallback([](const std::string& topic, const std::string& payload) {
            if (topic == "control.status") {
                std::cout << "\n[EVENT] " << topic << ": " << payload << '\n';
            }
        });
        client_->setSensorObservedCallback([this](const SensorObserved& sensor) {
            {
                std::lock_guard<std::mutex> lock(sensorMutex_);
                latestSensor_ = sensor;
                hasSensor_ = true;
            }
            ++sensorFrames_;
        });
    }

    void connect(int32_t leaseMs, bool readOnly) {
        if (!client_->connect(leaseMs)) {
            fail("connect");
        }

        std::string capabilities;
        if (!waitForCapabilities(capabilities, std::chrono::seconds(10))) {
            fail("getMotionCapabilities discovery");
        }
        std::cout << "[PASS] connected; no action has been started\n";

        std::string observedState;
        if (!client_->setObservedEnable(
                R"({"motionEnable":false,"sensorEnable":true})", observedState)) {
            std::cerr << "[WARN] enable SensorObserved failed, error="
                      << client_->getLastError() << '\n';
        }

        if (!readOnly) {
            takeControl();
        } else {
            std::cout << "[INFO] read-only mode; use 'take' before control commands\n";
        }
    }

    void run() {
        printHelp();
        bool running = true;
        while (!gStopping && running) {
            std::cout << "highlevel> " << std::flush;
            std::string line;
            while (!gStopping) {
                pollfd input = {STDIN_FILENO, POLLIN, 0};
                const int ready = ::poll(&input, 1, 100);
                if (ready > 0 && (input.revents & (POLLIN | POLLHUP))) {
                    if (!std::getline(std::cin, line)) running = false;
                    break;
                }
            }
            if (gStopping || !running) break;
            running = execute(trimLeft(line));
        }
    }

    void close() {
        std::string observedState;
        if (!client_->setObservedEnable(
                R"({"motionEnable":false,"sensorEnable":false})", observedState)) {
            std::cerr << "[WARN] disable SensorObserved failed, error="
                      << client_->getLastError() << '\n';
        }
        releaseControl();
        client_->disconnect();
    }

private:
    bool execute(const std::string& line) {
        if (line.empty()) return true;

        std::istringstream input(line);
        std::string command;
        input >> command;

        if (command == "help" || command == "?") {
            printHelp();
        } else if (command == "capabilities" || command == "caps") {
            query("capabilities", [this](std::string& out) {
                return client_->getMotionCapabilities(out);
            });
        } else if (command == "system") {
            query("system", [this](std::string& out) {
                return client_->querySystemStatus(out);
            });
        } else if (command == "state") {
            query("state", [this](std::string& out) {
                return client_->queryMotionState(out);
            });
        } else if (command == "motors") {
            printMotors();
        } else if (command == "status") {
            query("capabilities", [this](std::string& out) {
                return client_->getMotionCapabilities(out);
            });
            query("system", [this](std::string& out) {
                return client_->querySystemStatus(out);
            });
            query("state", [this](std::string& out) {
                return client_->queryMotionState(out);
            });
            printSensor(false);
        } else if (command == "take") {
            takeControl();
        } else if (command == "release") {
            releaseControl();
        } else if (command == "start") {
            std::string action;
            input >> action;
            const std::string params = rest(input);
            if (action.empty()) {
                std::cout << "[FAIL] usage: start ACTION [JSON]\n";
            } else if (!requireControl()) {
                return true;
            } else if (client_->startAction(action, params)) {
                actionActive_ = true;
                std::cout << "[PASS] started " << action << '\n';
            } else {
                printFailure("start " + action);
            }
        } else if (command == "set") {
            const std::string params = rest(input);
            if (params.empty()) {
                std::cout << "[FAIL] usage: set JSON\n";
            } else if (requireControl()) {
                result(client_->setActionParams(params), "params set; action remains active");
            }
        } else if (command == "send") {
            double seconds = 0.0;
            input >> seconds;
            const std::string params = rest(input);
            if (seconds <= 0.0 || params.empty()) {
                std::cout << "[FAIL] usage: send SECONDS JSON\n";
            } else if (requireControl() && client_->setActionParams(params)) {
                sleepFor(seconds);
                result(client_->setActionParams(kStopVelocity),
                       "timed command finished; walking velocity cleared");
            } else if (controlled()) {
                printFailure("set params");
            }
        } else if (command == "zero") {
            if (requireControl()) {
                result(client_->setActionParams(kStopVelocity),
                       "walking velocity cleared; action remains active");
            }
        } else if (command == "stop") {
            if (requireControl() && client_->stopAction()) {
                actionActive_ = false;
                std::cout << "[PASS] stop action requested\n";
            } else if (controlled()) {
                printFailure("stop action");
            }
        } else if (command == "estop") {
            if (requireControl()) result(client_->emergencyStop(), "emergency stop requested");
        } else if (command == "odom" || command == "sensor") {
            double seconds = command == "odom" ? 5.0 : 0.0;
            if (input >> seconds && seconds < 0.0) {
                std::cout << "[FAIL] seconds must be non-negative\n";
            } else {
                observe(seconds, command == "odom");
            }
        } else if (command == "quit" || command == "exit") {
            return false;
        } else {
            std::cout << "[FAIL] unknown command: " << command << " (use help)\n";
        }
        return true;
    }

    bool waitForCapabilities(std::string& output, std::chrono::seconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!gStopping) {
            if (client_->getMotionCapabilities(output)) return true;
            if (client_->getLastError() != Client::kRpcConnectFailed ||
                std::chrono::steady_clock::now() >= deadline) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }
        return false;
    }

    bool waitForState(Client::HighLevelState state, std::chrono::seconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        while (!gStopping && std::chrono::steady_clock::now() < deadline) {
            if (client_->getState() == state) return true;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        return client_->getState() == state;
    }

    void takeControl() {
        if (controlled()) {
            std::cout << "[INFO] control is already held\n";
            return;
        }
        if (!client_->startControl(10000) ||
            !waitForState(Client::kControlled, std::chrono::seconds(10))) {
            printFailure("take control");
            return;
        }
        std::cout << "[PASS] control acquired; no action has been started\n";
    }

    void releaseControl() {
        if (!controlled()) return;
        if (actionActive_) {
            if (client_->setActionParams(kStopVelocity)) {
                std::cout << "[cleanup] walking velocity cleared before release\n";
            } else {
                std::cerr << "[WARN] velocity clear before release failed, error="
                          << client_->getLastError() << '\n';
            }
            actionActive_ = false;
        }
        if (!client_->releaseControl()) {
            printFailure("release control");
            return;
        }
        waitForState(Client::kConnected, std::chrono::seconds(3));
        std::cout << "[PASS] control released\n";
    }

    bool controlled() const {
        return client_->getState() == Client::kControlled;
    }

    bool requireControl() const {
        if (controlled()) return true;
        std::cout << "[FAIL] High-level control is not held; run 'take' first\n";
        return false;
    }

    void sleepFor(double seconds) const {
        const auto deadline = std::chrono::steady_clock::now() +
                              std::chrono::duration<double>(seconds);
        while (!gStopping && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }

    template <typename Query>
    void query(const std::string& name, Query call) {
        std::string output;
        if (call(output)) std::cout << output << '\n';
        else printFailure(name);
    }

    void printMotors() {
        MotorLayout layout = {};
        if (!client_->getMotorLayout(layout)) {
            printFailure("motors");
            return;
        }
        std::cout << "motorNum=" << layout.motorNum << '\n';
        for (uint32_t i = 0; i < layout.motorNum; ++i) {
            const auto& motor = layout.motors[i];
            std::cout << "  limb=" << motor.limbNo << " joint=" << motor.jointNo
                      << " name=" << motor.name << '\n';
        }
    }

    void observe(double seconds, bool odomOnly) {
        const auto firstFrame = sensorFrames_.load();
        if (seconds > 0.0) {
            const auto deadline = std::chrono::steady_clock::now() +
                                  std::chrono::duration<double>(seconds);
            auto nextPrint = std::chrono::steady_clock::now();
            while (!gStopping && std::chrono::steady_clock::now() < deadline) {
                if (std::chrono::steady_clock::now() >= nextPrint) {
                    printSensor(odomOnly);
                    nextPrint = std::chrono::steady_clock::now() +
                                std::chrono::milliseconds(200);
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        } else {
            printSensor(odomOnly);
        }

        const auto received = sensorFrames_.load() - firstFrame;
        if (seconds > 0.0) {
            std::cout << "[INFO] SensorObserved frames=" << received
                      << " elapsed=" << seconds << "s rate="
                      << std::fixed << std::setprecision(2)
                      << (received / seconds) << "Hz\n";
        }
    }

    void printSensor(bool odomOnly) const {
        SensorObserved sensor = {};
        {
            std::lock_guard<std::mutex> lock(sensorMutex_);
            if (!hasSensor_) {
                std::cout << "[WAIT] no SensorObserved frame received\n";
                return;
            }
            sensor = latestSensor_;
        }
        const auto& odom = sensor.odom;
        std::cout << std::fixed << std::setprecision(3);
        if (!odomOnly) {
            std::cout << "sensor gps=" << static_cast<unsigned>(sensor.gps.valid)
                      << " uwb=" << static_cast<unsigned>(sensor.uwb.valid) << ' ';
        }
        std::cout << "odom valid=" << static_cast<unsigned>(odom.valid)
                  << " epoch=" << odom.epoch
                  << " pos=(" << odom.position[0] << ',' << odom.position[1] << ','
                  << odom.position[2] << ") yaw=" << odom.yaw
                  << " vel=(" << odom.velocity[0] << ',' << odom.velocity[1] << ','
                  << odom.velocity[2] << ") yawSpeed=" << odom.yawSpeed << '\n';
    }

    void result(bool ok, const std::string& success) {
        if (ok) std::cout << "[PASS] " << success << '\n';
        else printFailure(success);
    }

    void printFailure(const std::string& operation) const {
        std::cout << "[FAIL] " << operation << ", error="
                  << client_->getLastError() << '\n';
    }

    [[noreturn]] void fail(const std::string& operation) const {
        throw std::runtime_error(operation + " failed, error=" +
                                 std::to_string(client_->getLastError()));
    }

    static void printHelp() {
        std::cout
            << "Commands:\n"
            << "  status                       query capabilities, system, state and sensor\n"
            << "  capabilities | caps          list supported actions and parameters\n"
            << "  system                       query robot system status\n"
            << "  state                        query active action state; {} means no active action\n"
            << "  motors                       query motor layout\n"
            << "  odom [SECONDS]               print odometry at about 5 Hz (default 5s)\n"
            << "  sensor [SECONDS]             print GPS/UWB/odometry observation\n"
            << "  take                         acquire High-level control; starts no action\n"
            << "  release                      release High-level control\n"
            << "  start ACTION [JSON]           start an action\n"
            << "  set JSON                     keep action parameters active\n"
            << "  send SECONDS JSON             apply parameters, then clear walking velocity\n"
            << "  zero                         clear walking velocity; action keeps running\n"
            << "  stop                         stop the current RPC action\n"
            << "  estop                        request emergency stop\n"
            << "  quit                         clear velocity, release control and exit\n\n"
            << "Examples:\n"
            << "  start walking\n"
            << "  send 3 {\"lineVelocityX\":0.3,\"lineVelocityY\":0,\"velocity\":0}\n"
            << "  odom 5\n"
            << "  zero\n"
            << "  stop\n";
    }

    ClientPtr client_;
    mutable std::mutex sensorMutex_;
    SensorObserved latestSensor_ = {};
    bool hasSensor_ = false;
    std::atomic<uint64_t> sensorFrames_{0};
    bool actionActive_ = false;
};

}  // namespace

int main(int argc, char** argv) {
    Options options;
    if (!parseOptions(argc, argv, options)) {
        return (argc > 1 && (std::string(argv[1]) == "-h" ||
                             std::string(argv[1]) == "--help")) ? 0 : 2;
    }

    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    auto service = IMotionSdkService::instance();
    auto discoveryResults = std::make_shared<DiscoveryResults>();
    service->setDiscoverCallback(
        [discoveryResults](const std::string& sn, const std::string& info) {
            discoveryResults->add(sn, info);
        });
    service->setNetworkInterface(options.iface.c_str());
    if (!service->initialService(nullptr, options.clientId.c_str())) {
        std::cerr << "[FAIL] SDK init failed\n";
        return 1;
    }

    int status = 0;
    ClientPtr client;
    try {
        if (options.discoverOnly) {
            status = runDiscovery(service, discoveryResults) ? 0 : 1;
            service->shutdown();
            return status;
        }
        if (options.deviceId.empty() && service->isMultiDevice()) {
            throw std::runtime_error(
                "SDK multi-device mode requires --device-id SN (run --discover-only first)");
        }
        client = options.deviceId.empty() ? Client::create(false)
                                          : Client::create(options.deviceId);
        if (!client) throw std::runtime_error("create High-level client failed");

        HighLevelCli cli(client);
        cli.connect(options.leaseMs, options.readOnly);
        cli.run();
        cli.close();
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        if (client) client->disconnect();
        status = 1;
    }

    service->shutdown();
    return status;
}
