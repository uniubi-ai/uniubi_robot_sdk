/**
 * @file example_lowlevel.cpp
 * @brief Interactive Low-level SDK CLI for safe position-control demonstrations.
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <unistd.h>

#include "uniubi/robot_sdk/MotionLowLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"

using namespace uniubi::RobotSdk;

namespace {

using Client = IMotionLowLevelClient;
using ClientPtr = std::shared_ptr<Client>;
using LLState = Client::LowLevelState;
using LLError = Client::LowLevelError;

constexpr uint32_t kExpectedMotorCount = 12;
constexpr uint32_t kControlHz = 50;
constexpr uint32_t kObservationTimeoutMs = 10;
constexpr float kPoseDurationSeconds = 2.0f;
constexpr float kMaxStepRad = 0.25f;
constexpr float kMaxTrackingErrorRad = 0.25f;
constexpr float kDampingKd = 2.5f;
constexpr auto kControlPeriod = std::chrono::milliseconds(1000 / kControlHz);

std::atomic<bool> gStopping{false};

void onSignal(int) {
    gStopping.store(true);
}

struct Options {
    std::string clientId = "uniubi-lowlevel-cli";
    uint32_t observedHz = 500;
    uint32_t leaseMs = 60000;
};

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " [options]\n"
        << "  --client-id ID       SDK client id (default: uniubi-lowlevel-cli)\n"
        << "  --observed-hz HZ     Low-level observation rate (default: 500)\n"
        << "  --lease-ms MS        control lease in milliseconds (default: 60000)\n"
        << "  -h, --help           show this help\n\n"
        << "The program connects without enabling motor control and starts no pose.\n"
        << "Type 'help' at the lowlevel> prompt.\n";
}

bool parseUnsigned(const char* value, uint32_t& output) {
    try {
        const unsigned long parsed = std::stoul(value);
        if (parsed == 0 || parsed > 1000000UL) return false;
        output = static_cast<uint32_t>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

bool parseOptions(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printUsage(argv[0]);
            return false;
        }
        if (i + 1 >= argc) {
            std::cerr << "[FAIL] missing value for " << arg << '\n';
            return false;
        }
        if (arg == "--client-id") {
            options.clientId = argv[++i];
        } else if (arg == "--observed-hz") {
            if (!parseUnsigned(argv[++i], options.observedHz)) {
                std::cerr << "[FAIL] invalid --observed-hz\n";
                return false;
            }
        } else if (arg == "--lease-ms") {
            if (!parseUnsigned(argv[++i], options.leaseMs)) {
                std::cerr << "[FAIL] invalid --lease-ms\n";
                return false;
            }
        } else {
            std::cerr << "[FAIL] unknown option: " << arg << '\n';
            return false;
        }
    }
    return true;
}

class LowLevelCli {
public:
    explicit LowLevelCli(ClientPtr client) : client_(std::move(client)) {
        client_->setConnectCallback([this](LLState state, LLError error) {
            {
                std::lock_guard<std::mutex> lock(stateMutex_);
                lastState_ = state;
                lastError_ = error;
            }
            stateChanged_.notify_all();
        });
    }

    ~LowLevelCli() {
        stopController();
    }

    void connect(uint32_t observedHz, uint32_t leaseMs) {
        if (!client_->connect(observedHz, leaseMs)) fail("connect");
        if (!waitForState(LLState::kConnected, std::chrono::seconds(10))) {
            fail("wait connected");
        }
        if (!client_->getMotorLayout(layout_)) fail("get motor layout");
        layoutSupported_ = validateLayout();
        needsRestore_ = true;
        std::cout << "[PASS] connected; motor control is disabled and no pose has started\n";
        if (!layoutSupported_) {
            std::cout << "[WARN] pose commands require the standard DV500 12-joint layout\n";
        }
    }

    void run() {
        printHelp();
        bool running = true;
        while (!gStopping.load() && running) {
            std::cout << "lowlevel> " << std::flush;
            std::string line;
            while (!gStopping.load()) {
                pollfd input = {STDIN_FILENO, POLLIN, 0};
                const int ready = ::poll(&input, 1, 100);
                if (ready > 0 && (input.revents & (POLLIN | POLLHUP))) {
                    if (!std::getline(std::cin, line)) running = false;
                    break;
                }
            }
            if (!running || gStopping.load()) break;
            running = execute(line);
        }
    }

    void close() {
        releaseControl();
        if (needsRestore_ &&
            client_->getState() == static_cast<int32_t>(LLState::kConnected)) {
            if (client_->restoreMotionControlMode()) {
                std::cout << "[PASS] built-in motion control restored\n";
                needsRestore_ = false;
            } else {
                std::cerr << "[WARN] restore motion control failed, error="
                          << client_->getLastError() << '\n';
            }
        }
        client_->disconnect();
    }

private:
    enum class ControlMode { kDamping, kHold, kTrajectory };

    bool execute(const std::string& line) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command.empty()) return true;

        if (command == "help" || command == "?") {
            printHelp();
        } else if (command == "status") {
            printStatus();
        } else if (command == "motors") {
            printMotors();
        } else if (command == "stand") {
            runPose(false);
        } else if (command == "lie" || command == "lie-down") {
            runPose(true);
        } else if (command == "damping") {
            activateDamping();
        } else if (command == "release") {
            releaseControl();
        } else if (command == "estop") {
            requestEmergencyStop();
        } else if (command == "quit" || command == "exit") {
            return false;
        } else {
            std::cout << "[FAIL] unknown command: " << command << " (use help)\n";
        }
        return true;
    }

    bool validateLayout() const {
        if (layout_.motorNum != kExpectedMotorCount) return false;
        std::array<bool, kExpectedMotorCount> seen{};
        for (uint32_t i = 0; i < layout_.motorNum; ++i) {
            const auto& motor = layout_.motors[i];
            if (motor.limbNo >= 4 || motor.jointNo >= 3) return false;
            const uint32_t slot = motor.limbNo * 3 + motor.jointNo;
            if (seen[slot]) return false;
            seen[slot] = true;
        }
        return std::all_of(seen.begin(), seen.end(), [](bool value) { return value; });
    }

    static int observedIndex(const LowLevelMotionObserved& observation,
                             uint32_t limb, uint32_t joint) {
        for (uint32_t i = 0; i < observation.motorNum; ++i) {
            if (observation.motors[i].header.limbNo == limb &&
                observation.motors[i].header.jointNo == joint) {
                return static_cast<int>(i);
            }
        }
        return -1;
    }

    bool waitForState(LLState target, std::chrono::milliseconds timeout) {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(stateMutex_);
        while (client_->getState() != static_cast<int32_t>(target) &&
               std::chrono::steady_clock::now() < deadline) {
            stateChanged_.wait_for(lock, std::chrono::milliseconds(100));
        }
        return client_->getState() == static_cast<int32_t>(target);
    }

    bool prepared() const {
        return client_->getState() == static_cast<int32_t>(LLState::kPrepared);
    }

    bool ensureControl() {
        if (prepared() && controllerRunning_.load()) return true;
        if (!layoutSupported_) {
            std::cout << "[FAIL] unsupported motor layout; refusing to enable pose control\n";
            return false;
        }
        stopController();
        if (!prepared()) {
            if (!client_->setMotionEnable(true)) {
                printFailure("enable Low-level control");
                return false;
            }
            if (!waitForState(LLState::kPrepared, std::chrono::seconds(60))) {
                printFailure("wait Low-level prepared");
                client_->emergencyStop();
                client_->setMotionEnable(false);
                waitForState(LLState::kConnected, std::chrono::seconds(5));
                return false;
            }
        }

        LowLevelMotionObserved observation{};
        bool observationMatches = client_->getLatestObservation(&observation, 1000) &&
                                  observation.motorNum == layout_.motorNum;
        for (uint32_t i = 0; observationMatches && i < layout_.motorNum; ++i) {
            observationMatches = observedIndex(observation, layout_.motors[i].limbNo,
                                               layout_.motors[i].jointNo) >= 0;
        }
        if (!observationMatches) {
            client_->setMotionEnable(false);
            waitForState(LLState::kConnected, std::chrono::seconds(5));
            printFailure("initial observation");
            return false;
        }
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            latestObservation_ = observation;
            hasObservation_ = true;
            mode_ = ControlMode::kDamping;
            trajectoryDone_ = true;
        }
        controllerRunning_.store(true);
        controllerThread_ = std::thread(&LowLevelCli::controlLoop, this);
        std::cout << "[PASS] Low-level control enabled in damping mode; no pose has started\n";
        return true;
    }

    void releaseControl() {
        if (!prepared() && !controllerRunning_.load()) return;
        setDamping();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        stopController();
        if (prepared()) {
            if (!client_->setMotionEnable(false) ||
                !waitForState(LLState::kConnected, std::chrono::seconds(10))) {
                printFailure("release Low-level control");
                if (!client_->emergencyStop()) {
                    std::cerr << "[WARN] emergency-stop fallback failed, error="
                              << client_->getLastError() << '\n';
                }
                return;
            }
        }
        std::cout << "[PASS] Low-level control released\n";
    }

    void stopController() {
        controllerRunning_.store(false);
        controlChanged_.notify_all();
        if (controllerThread_.joinable()) controllerThread_.join();
    }

    void requestEmergencyStop() {
        stopController();
        const bool stopped = client_->emergencyStop();
        if (prepared()) {
            client_->setMotionEnable(false);
            waitForState(LLState::kConnected, std::chrono::seconds(5));
        }
        result(stopped, "emergency stop requested; controller stopped");
    }

    void setDamping() {
        if (!prepared() || !controllerRunning_.load()) {
            std::cout << "[FAIL] Low-level controller is not running\n";
            return;
        }
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            mode_ = ControlMode::kDamping;
            trajectoryDone_ = true;
        }
        controlChanged_.notify_all();
        std::cout << "[PASS] damping mode active\n";
    }

    void activateDamping() {
        if (!ensureControl()) return;
        setDamping();
    }

    void runPose(bool laying) {
        if (!ensureControl()) return;
        if (!layoutSupported_) {
            std::cout << "[FAIL] unsupported motor layout\n";
            return;
        }

        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            if (!hasObservation_) {
                std::cout << "[FAIL] no Low-level observation available\n";
                return;
            }
            for (uint32_t i = 0; i < layout_.motorNum; ++i) {
                const uint32_t limb = layout_.motors[i].limbNo;
                const uint32_t joint = layout_.motors[i].jointNo;
                const int observed = observedIndex(latestObservation_, limb, joint);
                if (observed < 0) {
                    std::cout << "[FAIL] latest observation does not match motor layout\n";
                    return;
                }
                startPosition_[i] = latestObservation_.motors[observed].position;
                targetPosition_[i] = posePosition(laying, limb, joint);
                commandPosition_[i] = startPosition_[i];
            }
            poseLaying_ = laying;
            trajectoryProgress_ = 0.0f;
            trajectoryDone_ = false;
            mode_ = ControlMode::kTrajectory;
        }
        controlChanged_.notify_all();

        std::unique_lock<std::mutex> lock(controlMutex_);
        controlChanged_.wait(lock, [&] {
            return trajectoryDone_ || !controllerRunning_.load() || gStopping.load();
        });
        if (trajectoryDone_ && mode_ == ControlMode::kHold) {
            std::cout << "[PASS] " << (laying ? "lie-down" : "stand")
                      << " pose reached and held\n";
        } else if (!gStopping.load()) {
            std::cout << "[FAIL] pose command interrupted\n";
        }
    }

    static float posePosition(bool laying, uint32_t limb, uint32_t joint) {
        if (!laying) {
            static constexpr float stand[3] = {0.0f, 0.8f, -1.5f};
            return stand[joint];
        }
        static constexpr float lie[3] = {0.48f, 1.10f, -2.72f};
        const float value = lie[joint];
        return (joint == 0 && (limb == 1 || limb == 3)) ? -value : value;
    }

    static float kpGain(uint32_t limb, uint32_t joint) {
        if (limb < 2) return 90.0f;
        return joint == 2 ? 140.0f : 130.0f;
    }

    static float kdGain(uint32_t limb) {
        return limb < 2 ? 1.5f : 2.5f;
    }

    void controlLoop() {
        auto nextCycle = std::chrono::steady_clock::now();
        while (controllerRunning_.load() && prepared()) {
            nextCycle += kControlPeriod;
            if (gStopping.load()) controlChanged_.notify_all();
            LowLevelMotionObserved observation{};
            if (!client_->getLatestObservation(&observation, kObservationTimeoutMs) ||
                observation.motorNum != layout_.motorNum) {
                ++observationFailures_;
                std::this_thread::sleep_until(nextCycle);
                continue;
            }

            MotorCtrlAction action{};
            LowLevelMotionCmd command{};
            action.motorNum = layout_.motorNum;
            {
                std::lock_guard<std::mutex> lock(controlMutex_);
                latestObservation_ = observation;
                hasObservation_ = true;

                if (mode_ == ControlMode::kTrajectory) advanceTrajectory(observation);
                const bool damping = mode_ == ControlMode::kDamping;
                for (uint32_t i = 0; i < layout_.motorNum; ++i) {
                    auto& motor = action.motors[i];
                    const uint32_t limb = layout_.motors[i].limbNo;
                    const uint32_t joint = layout_.motors[i].jointNo;
                    const int observed = observedIndex(observation, limb, joint);
                    if (observed < 0) {
                        controllerRunning_.store(false);
                        controlChanged_.notify_all();
                        return;
                    }
                    motor.position = damping ? observation.motors[observed].position
                                             : commandPosition_[i];
                    motor.velocity = 0.0f;
                    motor.kpGain = damping ? 0.0f : kpGain(limb, joint);
                    motor.kdGain = damping ? kDampingKd : kdGain(limb);
                    motor.torque = 0.0f;
                    motor.header.limbNo = limb;
                    motor.header.jointNo = joint;
                }
                if (damping) {
                    command.action = -1;
                    std::snprintf(command.acName, sizeof(command.acName), "%s", "damping");
                } else {
                    command.action = poseLaying_ ? 0 : 1;
                    std::snprintf(command.acName, sizeof(command.acName), "%s",
                                  poseLaying_ ? "laying" : "standing");
                }
            }

            if (!client_->sendControl(action, &command)) {
                std::cerr << "\n[FAIL] sendControl failed, error="
                          << client_->getLastError() << '\n';
                controllerRunning_.store(false);
                controlChanged_.notify_all();
                break;
            }
            std::this_thread::sleep_until(nextCycle);
        }
        controllerRunning_.store(false);
        controlChanged_.notify_all();
    }

    void advanceTrajectory(const LowLevelMotionObserved& observation) {
        float maxTrackingError = 0.0f;
        for (uint32_t i = 0; i < layout_.motorNum; ++i) {
            const int observed = observedIndex(observation, layout_.motors[i].limbNo,
                                               layout_.motors[i].jointNo);
            if (observed < 0) return;
            maxTrackingError = std::max(maxTrackingError,
                std::fabs(observation.motors[observed].position - commandPosition_[i]));
        }
        if (maxTrackingError > kMaxTrackingErrorRad) return;

        trajectoryProgress_ = std::min(1.0f,
            trajectoryProgress_ + 1.0f / (kPoseDurationSeconds * kControlHz));
        const float smooth = trajectoryProgress_ * trajectoryProgress_ *
                             (3.0f - 2.0f * trajectoryProgress_);
        bool clamped = false;
        for (uint32_t i = 0; i < layout_.motorNum; ++i) {
            const float reference = startPosition_[i] +
                                    (targetPosition_[i] - startPosition_[i]) * smooth;
            const float lower = commandPosition_[i] - kMaxStepRad;
            const float upper = commandPosition_[i] + kMaxStepRad;
            const float next = std::max(lower, std::min(reference, upper));
            clamped = clamped || std::fabs(next - reference) > 1e-6f;
            commandPosition_[i] = next;
        }
        if (trajectoryProgress_ >= 1.0f && !clamped) {
            mode_ = ControlMode::kHold;
            trajectoryDone_ = true;
            controlChanged_.notify_all();
        }
    }

    void printStatus() {
        LowLevelMotionObserved observation{};
        bool hasObservation = false;
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            if (hasObservation_) {
                observation = latestObservation_;
                hasObservation = true;
            }
        }
        std::cout << "state=" << client_->getState()
                  << " error=" << client_->getLastError()
                  << " prepared=" << (prepared() ? "yes" : "no")
                  << " controller=" << (controllerRunning_.load() ? "running" : "stopped")
                  << " obs_failures=" << observationFailures_.load() << '\n';
        if (hasObservation) {
            std::cout << std::fixed << std::setprecision(3)
                      << "imu gyro=(" << observation.imu.gyro.x << ','
                      << observation.imu.gyro.y << ',' << observation.imu.gyro.z
                      << ") power=" << observation.power.chargeVoltage << "V\n";
        }
    }

    void printMotors() {
        LowLevelMotionObserved observation{};
        bool hasObservation = false;
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            if (hasObservation_) {
                observation = latestObservation_;
                hasObservation = true;
            }
        }
        std::cout << "motorNum=" << layout_.motorNum << '\n';
        for (uint32_t i = 0; i < layout_.motorNum; ++i) {
            const auto& motor = layout_.motors[i];
            std::cout << "  [" << i << "] limb=" << motor.limbNo
                      << " joint=" << motor.jointNo << " name=" << motor.name;
            const int observedIndexValue = hasObservation
                ? observedIndex(observation, motor.limbNo, motor.jointNo) : -1;
            if (observedIndexValue >= 0) {
                const auto& observed = observation.motors[observedIndexValue];
                std::cout << std::fixed << std::setprecision(3)
                          << " pos=" << observed.position << " vel=" << observed.velocity
                          << " enabled=" << static_cast<unsigned>(observed.enable)
                          << " online=" << static_cast<unsigned>(observed.online)
                          << " error=" << static_cast<unsigned>(observed.error);
            }
            std::cout << '\n';
        }
    }

    void result(bool ok, const std::string& success) const {
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
            << "  status              show connection, controller and observation summary\n"
            << "  motors              show the motor layout and latest joint state\n"
            << "  stand               smoothly move to and hold the standing pose\n"
            << "  lie | lie-down      smoothly move to and hold the lying pose\n"
            << "  damping             enable zero stiffness with velocity damping\n"
            << "  release             damping, stop the controller and disable motor control\n"
            << "  estop               request emergency stop\n"
            << "  quit                 release, restore built-in motion control and exit\n\n"
            << "No pose starts automatically. Pose commands enable Low-level control on demand.\n";
    }

    ClientPtr client_;
    MotorLayout layout_{};
    bool layoutSupported_ = false;
    bool needsRestore_ = false;

    std::mutex stateMutex_;
    std::condition_variable stateChanged_;
    LLState lastState_ = LLState::kDisconnected;
    LLError lastError_ = LLError::kNone;

    std::atomic<bool> controllerRunning_{false};
    std::atomic<uint64_t> observationFailures_{0};
    std::thread controllerThread_;
    std::mutex controlMutex_;
    std::condition_variable controlChanged_;
    ControlMode mode_ = ControlMode::kDamping;
    LowLevelMotionObserved latestObservation_{};
    bool hasObservation_ = false;
    bool poseLaying_ = false;
    bool trajectoryDone_ = true;
    float trajectoryProgress_ = 0.0f;
    std::array<float, kExpectedMotorCount> startPosition_{};
    std::array<float, kExpectedMotorCount> targetPosition_{};
    std::array<float, kExpectedMotorCount> commandPosition_{};
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
    if (!service->initialService(nullptr, options.clientId.c_str())) {
        std::cerr << "[FAIL] SDK init failed\n";
        return 1;
    }

    int status = 0;
    ClientPtr client;
    try {
        client = Client::create();
        if (!client) throw std::runtime_error("create Low-level client failed");
        LowLevelCli cli(client);
        cli.connect(options.observedHz, options.leaseMs);
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
