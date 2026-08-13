/**
 * @file example_lowlevel_tensorrt.cpp
 * @brief On-board Low-level TensorRT policy example for Jetson Orin.
 *
 * The example builds an FP32 TensorRT engine from ONNX at every startup.
 * It intentionally has no PyTorch or ONNX Runtime dependency.
 */

#include <NvInfer.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <csignal>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

#include "uniubi/robot_sdk/MotionLowLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"

using namespace uniubi::RobotSdk;

namespace {

using Client = IMotionLowLevelClient;
using ClientPtr = std::shared_ptr<Client>;
using LLState = Client::LowLevelState;

constexpr uint32_t kMotorCount = 12;
constexpr uint32_t kControlHz = 50;
constexpr uint32_t kObservationTimeoutMs = 10;
constexpr auto kControlPeriod = std::chrono::milliseconds(1000 / kControlHz);

const std::array<const char*, kMotorCount> kSdkJointOrder{{
    "FL_ABAD", "FL_HIP", "FL_KNEE",
    "FR_ABAD", "FR_HIP", "FR_KNEE",
    "RL_ABAD", "RL_HIP", "RL_KNEE",
    "RR_ABAD", "RR_HIP", "RR_KNEE",
}};

// This example model was trained/exported in joint-major order. This is a
// model contract, not an SDK contract; another model may use another order.
const std::array<const char*, kMotorCount> kModelJointOrder{{
    "FL_ABAD", "FR_ABAD", "RL_ABAD", "RR_ABAD",
    "FL_HIP", "FR_HIP", "RL_HIP", "RR_HIP",
    "FL_KNEE", "FR_KNEE", "RL_KNEE", "RR_KNEE",
}};

// model[i] = sdk[kModelIndexToSdkIndex[i]],
// sdk[i] = model[kSdkIndexToModelIndex[i]].
const std::array<size_t, kMotorCount> kModelIndexToSdkIndex{{
    0, 3, 6, 9, 1, 4, 7, 10, 2, 5, 8, 11,
}};
const std::array<size_t, kMotorCount> kSdkIndexToModelIndex{{
    0, 4, 8, 1, 5, 9, 2, 6, 10, 3, 7, 11,
}};

const std::array<float, kMotorCount> kStandSdk{{
    0.0f, 0.8f, -1.58f, 0.0f, 0.8f, -1.58f,
    0.0f, 0.8f, -1.58f, 0.0f, 0.8f, -1.58f,
}};
const std::array<float, kMotorCount> kLaySdk{{
    0.48f, 1.10f, -2.72f, -0.48f, 1.10f, -2.72f,
    0.48f, 1.10f, -2.72f, -0.48f, 1.10f, -2.72f,
}};
const std::array<float, kMotorCount> kPostureKp{{
    90.0f, 90.0f, 90.0f, 90.0f, 90.0f, 90.0f,
    130.0f, 130.0f, 140.0f, 130.0f, 130.0f, 140.0f,
}};
const std::array<float, kMotorCount> kPostureKd{{
    1.5f, 1.5f, 1.5f, 1.5f, 1.5f, 1.5f,
    2.5f, 2.5f, 2.5f, 2.5f, 2.5f, 2.5f,
}};

std::atomic<bool> gStopping{false};

void onSignal(int) {
    gStopping.store(true);
}

template <typename T>
using TrtPtr = std::unique_ptr<T>;

class TrtLogger final : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* message) noexcept override {
        if (severity <= Severity::kWARNING) {
            std::cerr << "[TensorRT] " << message << '\n';
        }
    }
};

void checkCuda(cudaError_t status, const char* operation) {
    if (status != cudaSuccess) {
        throw std::runtime_error(std::string(operation) + " failed: " +
                                 cudaGetErrorString(status));
    }
}

bool dimsEqual(const nvinfer1::Dims& dims, int first, int second) {
    return dims.nbDims == 2 && dims.d[0] == first && dims.d[1] == second;
}

class TensorRTPolicy {
public:
    TensorRTPolicy(const std::string& onnxPath, size_t workspaceMiB) {
        builder_.reset(nvinfer1::createInferBuilder(logger_));
        if (!builder_) throw std::runtime_error("createInferBuilder failed");
        // TensorRT 10 networks are always explicit-batch. Passing the former
        // kEXPLICIT_BATCH flag is deprecated and has no effect.
        network_.reset(builder_->createNetworkV2(0U));
        if (!network_) throw std::runtime_error("createNetworkV2 failed");
        parser_.reset(nvonnxparser::createParser(*network_, logger_));
        if (!parser_) throw std::runtime_error("create ONNX parser failed");
        if (!parser_->parseFromFile(onnxPath.c_str(),
                                    static_cast<int>(nvinfer1::ILogger::Severity::kWARNING))) {
            std::ostringstream error;
            error << "parse ONNX failed: " << onnxPath;
            for (int32_t i = 0; i < parser_->getNbErrors(); ++i) {
                error << "\n" << parser_->getError(i)->desc();
            }
            throw std::runtime_error(error.str());
        }
        if (network_->getNbInputs() != 1 || network_->getNbOutputs() != 1) {
            throw std::runtime_error("expected exactly one ONNX input and one output");
        }
        if (!dimsEqual(network_->getInput(0)->getDimensions(), 1, 45) ||
            !dimsEqual(network_->getOutput(0)->getDimensions(), 1, 12)) {
            throw std::runtime_error("expected static ONNX shapes [1,45] -> [1,12]");
        }

        config_.reset(builder_->createBuilderConfig());
        if (!config_) throw std::runtime_error("createBuilderConfig failed");
        // TensorRT enables TF32 by default on supported GPUs. Clear it so this
        // example's build contract is strict IEEE FP32 rather than TF32.
        config_->clearFlag(nvinfer1::BuilderFlag::kTF32);
        config_->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE,
                                    std::max<size_t>(workspaceMiB, 1) * 1024U * 1024U);
        std::cout << "[INFO] building TensorRT engine from " << onnxPath
                  << " precision=FP32 workspace=" << workspaceMiB << " MiB\n";
        plan_.reset(builder_->buildSerializedNetwork(*network_, *config_));
        if (!plan_) throw std::runtime_error("buildSerializedNetwork failed");
        runtime_.reset(nvinfer1::createInferRuntime(logger_));
        if (!runtime_) throw std::runtime_error("createInferRuntime failed");
        engine_.reset(runtime_->deserializeCudaEngine(plan_->data(), plan_->size()));
        if (!engine_) throw std::runtime_error("deserializeCudaEngine failed");
        context_.reset(engine_->createExecutionContext());
        if (!context_) throw std::runtime_error("createExecutionContext failed");

        for (int32_t i = 0; i < engine_->getNbIOTensors(); ++i) {
            const char* name = engine_->getIOTensorName(i);
            if (engine_->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
                if (!inputName_.empty()) throw std::runtime_error("multiple TensorRT inputs");
                inputName_ = name;
            } else {
                if (!outputName_.empty()) throw std::runtime_error("multiple TensorRT outputs");
                outputName_ = name;
            }
        }
        if (inputName_.empty() || outputName_.empty()) {
            throw std::runtime_error("TensorRT input/output missing");
        }
        if (!context_->setInputShape(inputName_.c_str(), nvinfer1::Dims2{1, 45}) ||
            !dimsEqual(context_->getTensorShape(inputName_.c_str()), 1, 45) ||
            !dimsEqual(context_->getTensorShape(outputName_.c_str()), 1, 12)) {
            throw std::runtime_error("TensorRT engine shape mismatch");
        }
        if (engine_->getTensorDataType(inputName_.c_str()) != nvinfer1::DataType::kFLOAT ||
            engine_->getTensorDataType(outputName_.c_str()) != nvinfer1::DataType::kFLOAT) {
            throw std::runtime_error("TensorRT example requires FP32 input and output tensors");
        }

        checkCuda(cudaStreamCreate(&stream_), "cudaStreamCreate");
        try {
            checkCuda(cudaMalloc(&deviceInput_, 45 * sizeof(float)), "cudaMalloc(input)");
            checkCuda(cudaMalloc(&deviceOutput_, 12 * sizeof(float)), "cudaMalloc(output)");
            if (!context_->setTensorAddress(inputName_.c_str(), deviceInput_) ||
                !context_->setTensorAddress(outputName_.c_str(), deviceOutput_)) {
                throw std::runtime_error("setTensorAddress failed");
            }
        } catch (...) {
            releaseCuda();
            throw;
        }
    }

    ~TensorRTPolicy() {
        releaseCuda();
    }

    TensorRTPolicy(const TensorRTPolicy&) = delete;
    TensorRTPolicy& operator=(const TensorRTPolicy&) = delete;

    std::array<float, 12> infer(const std::array<float, 45>& input) {
        std::array<float, 12> output{};
        checkCuda(cudaMemcpyAsync(deviceInput_, input.data(), 45 * sizeof(float),
                                  cudaMemcpyHostToDevice, stream_),
                  "cudaMemcpyAsync(H2D)");
        if (!context_->enqueueV3(stream_)) {
            throw std::runtime_error("TensorRT enqueueV3 failed");
        }
        checkCuda(cudaMemcpyAsync(output.data(), deviceOutput_, 12 * sizeof(float),
                                  cudaMemcpyDeviceToHost, stream_),
                  "cudaMemcpyAsync(D2H)");
        checkCuda(cudaStreamSynchronize(stream_), "cudaStreamSynchronize");
        return output;
    }

private:
    void releaseCuda() noexcept {
        if (deviceInput_) cudaFree(deviceInput_);
        if (deviceOutput_) cudaFree(deviceOutput_);
        if (stream_) cudaStreamDestroy(stream_);
        deviceInput_ = nullptr;
        deviceOutput_ = nullptr;
        stream_ = nullptr;
    }

    TrtLogger logger_;
    TrtPtr<nvinfer1::IBuilder> builder_;
    TrtPtr<nvinfer1::INetworkDefinition> network_;
    TrtPtr<nvonnxparser::IParser> parser_;
    TrtPtr<nvinfer1::IBuilderConfig> config_;
    TrtPtr<nvinfer1::IHostMemory> plan_;
    TrtPtr<nvinfer1::IRuntime> runtime_;
    TrtPtr<nvinfer1::ICudaEngine> engine_;
    TrtPtr<nvinfer1::IExecutionContext> context_;
    std::string inputName_;
    std::string outputName_;
    cudaStream_t stream_ = nullptr;
    void* deviceInput_ = nullptr;
    void* deviceOutput_ = nullptr;
};

struct Options {
    std::string onnxPath;
    std::string clientId = "uniubi-lowlevel-tensorrt";
    uint32_t observedHz = 500;
    uint32_t leaseMs = 60000;
    size_t workspaceMiB = 512;
    bool validateOnly = false;
};

void printUsage(const char* program) {
    std::cout
        << "Usage: " << program << " --onnx MODEL [options]\n"
        << "  --onnx PATH          static [1,45] -> [1,12] ONNX policy\n"
        << "  --workspace-mib N    TensorRT build workspace (default: 512)\n"
        << "  --observed-hz HZ     Low-level observation rate (default: 500)\n"
        << "  --lease-ms MS        control lease (default: 60000)\n"
        << "  --client-id ID       SDK client id\n"
        << "  --validate-only      build and infer zeros without initializing SDK\n"
        << "  -h, --help           show this help\n";
}

bool parsePositive(const char* value, uint64_t& output) {
    try {
        const auto parsed = std::stoull(value);
        if (parsed == 0) return false;
        output = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

bool parseOptions(int argc, char** argv, Options& options, bool& help) {
    help = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            help = true;
            return false;
        }
        if (arg == "--validate-only") {
            options.validateOnly = true;
            continue;
        }
        if (i + 1 >= argc) {
            std::cerr << "[FAIL] missing value for " << arg << '\n';
            return false;
        }
        const char* value = argv[++i];
        uint64_t parsed = 0;
        if (arg == "--onnx") {
            options.onnxPath = value;
        } else if (arg == "--client-id") {
            options.clientId = value;
        } else if (arg == "--workspace-mib") {
            if (!parsePositive(value, parsed)) return false;
            options.workspaceMiB = static_cast<size_t>(parsed);
        } else if (arg == "--observed-hz") {
            if (!parsePositive(value, parsed) || parsed > 10000) return false;
            options.observedHz = static_cast<uint32_t>(parsed);
        } else if (arg == "--lease-ms") {
            if (!parsePositive(value, parsed) || parsed > 10000000) return false;
            options.leaseMs = static_cast<uint32_t>(parsed);
        } else {
            std::cerr << "[FAIL] unknown option: " << arg << '\n';
            return false;
        }
    }
    if (options.onnxPath.empty()) {
        std::cerr << "[FAIL] --onnx is required\n";
        return false;
    }
    return true;
}

int observedIndex(const LowLevelMotionObserved& observation,
                  uint32_t limb, uint32_t joint) {
    for (uint32_t i = 0; i < observation.motorNum; ++i) {
        if (observation.motors[i].header.limbNo == limb &&
            observation.motors[i].header.jointNo == joint) {
            return static_cast<int>(i);
        }
    }
    return -1;
}

std::array<float, 3> rotateInverse(const Quaternionf& q,
                                   const std::array<float, 3>& value) {
    float w = q.w, x = q.x, y = q.y, z = q.z;
    const float norm = std::sqrt(w * w + x * x + y * y + z * z);
    if (norm < 1e-6f) return value;
    w /= norm;
    x = -x / norm;
    y = -y / norm;
    z = -z / norm;
    const std::array<float, 3> uv{{y * value[2] - z * value[1],
                                  z * value[0] - x * value[2],
                                  x * value[1] - y * value[0]}};
    const std::array<float, 3> uuv{{y * uv[2] - z * uv[1],
                                   z * uv[0] - x * uv[2],
                                   x * uv[1] - y * uv[0]}};
    return {{value[0] + 2.0f * (w * uv[0] + uuv[0]),
             value[1] + 2.0f * (w * uv[1] + uuv[1]),
             value[2] + 2.0f * (w * uv[2] + uuv[2])}};
}

class LowLevelTensorRTCli {
public:
    LowLevelTensorRTCli(ClientPtr client, TensorRTPolicy& policy)
        : client_(std::move(client)), policy_(policy) {
        client_->setConnectCallback([this](LLState state, Client::LowLevelError error) {
            std::cout << "[callback] state=" << static_cast<int>(state)
                      << " error=" << static_cast<int>(error) << '\n';
            stateChanged_.notify_all();
        });
    }

    ~LowLevelTensorRTCli() {
        close();
    }

    void connect(uint32_t observedHz, uint32_t leaseMs) {
        if (!client_->connect(observedHz, leaseMs) ||
            !waitForState(LLState::kConnected, std::chrono::seconds(10))) {
            fail("connect");
        }
        if (!client_->getMotorLayout(layout_)) fail("getMotorLayout");
        validateMotorLayout();
        running_.store(true);
        controlThread_ = std::thread(&LowLevelTensorRTCli::controlLoop, this);
        std::cout << "[PASS] connected; no control is enabled and no pose has started\n";
    }

    void run() {
        printHelp();
        std::string line;
        while (!gStopping.load()) {
            std::cout << "lowlevel> " << std::flush;
            if (!std::getline(std::cin, line)) break;
            if (!execute(line)) break;
        }
    }

    void close() noexcept {
        if (closed_.exchange(true)) return;
        running_.store(false);
        if (controlThread_.joinable()) controlThread_.join();
        if (client_ && client_->getState() == static_cast<int32_t>(LLState::kPrepared)) {
            client_->setMotionEnable(false);
            waitForState(LLState::kConnected, std::chrono::seconds(10));
        }
        if (client_) client_->disconnect();
    }

private:
    enum class Mode { kIdle, kHold, kWalk };

    void validateMotorLayout() {
        if (layout_.motorNum != kMotorCount) {
            throw std::runtime_error("MotorLayout must contain exactly 12 motors");
        }
        for (uint32_t i = 0; i < kMotorCount; ++i) {
            const uint16_t expectedLimb = static_cast<uint16_t>(i / 3);
            const uint16_t expectedJoint = static_cast<uint16_t>(i % 3);
            const auto& actual = layout_.motors[i];
            if (actual.limbNo != expectedLimb || actual.jointNo != expectedJoint) {
                std::ostringstream error;
                error << "unsupported MotorLayout order at index " << i
                      << ": expected=(" << expectedLimb << ',' << expectedJoint
                      << ") actual=(" << actual.limbNo << ',' << actual.jointNo
                      << "); refusing to enable Low-level control";
                throw std::runtime_error(error.str());
            }
        }
        std::cout << "[PASS] MotorLayout SDK order: ";
        printOrder(kSdkJointOrder);
        std::cout << "[INFO] policy model order: ";
        printOrder(kModelJointOrder);
    }

    template <size_t N>
    static void printOrder(const std::array<const char*, N>& order) {
        for (size_t i = 0; i < order.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << order[i];
        }
        std::cout << '\n';
    }

    bool execute(const std::string& line) {
        std::istringstream input(line);
        std::string command;
        input >> command;
        if (command.empty()) return true;
        if (command == "help" || command == "?") {
            printHelp();
        } else if (command == "state") {
            printState();
        } else if (command == "obs") {
            printObservation();
        } else if (command == "stand") {
            transitionTo(kStandSdk, 2.0f, "stand");
            posture_ = "standing";
        } else if (command == "walk") {
            std::array<float, 3> values{{0.5f, 0.0f, 0.0f}};
            if (input >> values[0]) {
                if (!(input >> values[1] >> values[2])) {
                    std::cout << "[FAIL] usage: walk [VX VY YAW]\n";
                    return true;
                }
            }
            if (posture_ != "standing" && posture_ != "walking") {
                transitionTo(kStandSdk, 2.0f, "walk prepare");
            } else {
                ensurePrepared();
            }
            {
                std::lock_guard<std::mutex> lock(controlMutex_);
                command_ = values;
                lastAction_.fill(0.0f);
                mode_ = Mode::kWalk;
            }
            posture_ = "walking";
            std::cout << "[PASS] policy running command=" << values[0] << ','
                      << values[1] << ',' << values[2] << '\n';
        } else if (command == "stop") {
            requirePrepared();
            transitionTo(kStandSdk, 1.0f, "stop");
            posture_ = "standing";
        } else if (command == "lay" || command == "lie") {
            requirePrepared();
            transitionTo(kLaySdk, 2.0f, "lay");
            posture_ = "laying";
        } else if (command == "quit" || command == "exit") {
            return false;
        } else {
            std::cout << "[FAIL] unknown command: " << command << '\n';
        }
        return true;
    }

    bool waitForState(LLState target, std::chrono::milliseconds timeout) const {
        const auto deadline = std::chrono::steady_clock::now() + timeout;
        std::unique_lock<std::mutex> lock(stateMutex_);
        while (client_->getState() != static_cast<int32_t>(target)) {
            if (std::chrono::steady_clock::now() >= deadline) return false;
            stateChanged_.wait_for(lock, std::chrono::milliseconds(100));
        }
        return true;
    }

    void ensurePrepared() {
        if (client_->getState() == static_cast<int32_t>(LLState::kConnected)) {
            if (!client_->setMotionEnable(true) ||
                !waitForState(LLState::kPrepared, std::chrono::seconds(60))) {
                fail("enable Low-level control");
            }
        }
        requirePrepared();
    }

    void requirePrepared() const {
        if (client_->getState() != static_cast<int32_t>(LLState::kPrepared)) {
            throw std::runtime_error("run stand before this command");
        }
    }

    std::array<float, kMotorCount> observedSdkPositions(
        const LowLevelMotionObserved& observation) const {
        if (observation.motorNum != kMotorCount) {
            throw std::runtime_error("observation must contain exactly 12 motors");
        }
        std::array<float, kMotorCount> positions{};
        for (uint32_t i = 0; i < kMotorCount; ++i) {
            const auto& motor = layout_.motors[i];
            const int index = observedIndex(observation, motor.limbNo, motor.jointNo);
            if (index < 0) throw std::runtime_error("observation does not match MotorLayout");
            positions[i] = observation.motors[index].position;
        }
        return positions;
    }

    void transitionTo(const std::array<float, kMotorCount>& target,
                      float durationSeconds, const char* name) {
        ensurePrepared();
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            mode_ = Mode::kIdle;
        }
        std::this_thread::sleep_for(kControlPeriod * 2);
        LowLevelMotionObserved observation{};
        if (!client_->getLatestObservation(&observation, 500)) {
            fail("get initial observation");
        }
        const auto start = observedSdkPositions(observation);
        const uint32_t steps = std::max<uint32_t>(1,
            static_cast<uint32_t>(std::ceil(durationSeconds * kControlHz)));
        auto next = std::chrono::steady_clock::now();
        for (uint32_t step = 0; step < steps && !gStopping.load(); ++step) {
            const float ratio = static_cast<float>(step + 1) / steps;
            std::array<float, kMotorCount> pose{};
            for (size_t i = 0; i < pose.size(); ++i) {
                pose[i] = start[i] + ratio * (target[i] - start[i]);
            }
            if (!client_->sendControl(makeAction(pose, kPostureKp, kPostureKd))) {
                fail(std::string(name) + " sendControl");
            }
            next += kControlPeriod;
            std::this_thread::sleep_until(next);
        }
        {
            std::lock_guard<std::mutex> lock(controlMutex_);
            holdPose_ = target;
            mode_ = Mode::kHold;
        }
        std::cout << "[PASS] " << name << " transition sent count=" << steps
                  << " duration=" << durationSeconds << "s\n";
    }

    MotorCtrlAction makeAction(const std::array<float, kMotorCount>& positions,
                               const std::array<float, kMotorCount>& kp,
                               const std::array<float, kMotorCount>& kd) const {
        MotorCtrlAction action{};
        action.motorNum = layout_.motorNum;
        for (uint32_t i = 0; i < layout_.motorNum; ++i) {
            auto& output = action.motors[i];
            output.position = positions[i];
            output.velocity = 0.0f;
            output.kpGain = kp[i];
            output.kdGain = kd[i];
            output.torque = 0.0f;
            output.header.limbNo = layout_.motors[i].limbNo;
            output.header.jointNo = layout_.motors[i].jointNo;
        }
        return action;
    }

    std::array<float, 45> buildPolicyObservation(
        const LowLevelMotionObserved& observation,
        const std::array<float, 3>& command,
        const std::array<float, 12>& lastAction) const {
        const auto sdkPosition = observedSdkPositions(observation);
        std::array<float, kMotorCount> sdkVelocity{};
        for (uint32_t i = 0; i < kMotorCount; ++i) {
            const auto& motor = layout_.motors[i];
            const int index = observedIndex(observation, motor.limbNo, motor.jointNo);
            if (index < 0) throw std::runtime_error("observation does not match MotorLayout");
            sdkVelocity[i] = observation.motors[index].velocity;
        }
        const auto gravity = rotateInverse(observation.imu.quaternion, {{0.0f, 0.0f, -1.0f}});
        std::array<float, 45> result{};
        size_t offset = 0;
        result[offset++] = observation.imu.gyro.x * 0.2f;
        result[offset++] = observation.imu.gyro.y * 0.2f;
        result[offset++] = observation.imu.gyro.z * 0.2f;
        for (float value : gravity) result[offset++] = value;
        for (float value : command) result[offset++] = value;
        for (size_t model = 0; model < kMotorCount; ++model) {
            const size_t sdk = kModelIndexToSdkIndex[model];
            result[offset++] = sdkPosition[sdk] - kStandSdk[sdk];
        }
        for (size_t model = 0; model < kMotorCount; ++model) {
            result[offset++] = sdkVelocity[kModelIndexToSdkIndex[model]] * 0.05f;
        }
        for (float value : lastAction) result[offset++] = value;
        if (offset != result.size()) throw std::runtime_error("policy observation size mismatch");
        return result;
    }

    void controlLoop() {
        auto next = std::chrono::steady_clock::now();
        while (running_.load()) {
            next += kControlPeriod;
            Mode mode;
            std::array<float, kMotorCount> hold;
            std::array<float, 3> command;
            std::array<float, kMotorCount> lastAction;
            {
                std::lock_guard<std::mutex> lock(controlMutex_);
                mode = mode_;
                hold = holdPose_;
                command = command_;
                lastAction = lastAction_;
            }
            try {
                if (mode == Mode::kHold) {
                    if (client_->sendControl(makeAction(hold, kPostureKp, kPostureKd))) {
                        ++sent_;
                    } else {
                        ++failed_;
                    }
                } else if (mode == Mode::kWalk) {
                    LowLevelMotionObserved observation{};
                    if (!client_->getLatestObservation(&observation, kObservationTimeoutMs)) {
                        ++failed_;
                    } else {
                        const auto policyInput = buildPolicyObservation(observation, command, lastAction);
                        auto actionModel = policy_.infer(policyInput);
                        std::array<float, kMotorCount> targetSdk{};
                        for (size_t sdk = 0; sdk < kMotorCount; ++sdk) {
                            const size_t model = kSdkIndexToModelIndex[sdk];
                            const float clipped = std::max(-100.0f,
                                std::min(100.0f, actionModel[model]));
                            targetSdk[sdk] = kStandSdk[sdk] + 0.25f * clipped;
                            actionModel[model] = clipped;
                        }
                        std::array<float, kMotorCount> kp{};
                        std::array<float, kMotorCount> kd{};
                        kp.fill(35.0f);
                        kd.fill(1.0f);
                        if (client_->sendControl(makeAction(targetSdk, kp, kd))) ++sent_;
                        else ++failed_;
                        std::lock_guard<std::mutex> lock(controlMutex_);
                        lastAction_ = actionModel;
                    }
                }
            } catch (const std::exception& error) {
                ++failed_;
                std::cerr << "\n[FAIL] control loop: " << error.what() << '\n';
                std::lock_guard<std::mutex> lock(controlMutex_);
                mode_ = Mode::kIdle;
            }
            std::this_thread::sleep_until(next);
        }
    }

    void printState() const {
        std::lock_guard<std::mutex> lock(controlMutex_);
        std::cout << "state=" << client_->getState()
                  << " error=" << client_->getLastError()
                  << " posture=" << posture_
                  << " mode=" << static_cast<int>(mode_)
                  << " sent=" << sent_.load()
                  << " failed=" << failed_.load() << '\n';
    }

    void printObservation() const {
        LowLevelMotionObserved observation{};
        if (!client_->getLatestObservation(&observation, 500)) {
            std::cout << "[WAIT] no observation\n";
            return;
        }
        const auto positions = observedSdkPositions(observation);
        std::cout << std::fixed << std::setprecision(3) << "q=";
        for (float value : positions) std::cout << ' ' << value;
        std::cout << '\n';
    }

    [[noreturn]] void fail(const std::string& operation) const {
        throw std::runtime_error(operation + " failed, sdk_error=" +
                                 std::to_string(client_->getLastError()));
    }

    static void printHelp() {
        std::cout
            << "Commands:\n"
            << "  stand                 smoothly stand from measured position\n"
            << "  walk [VX VY YAW]      run the TensorRT policy\n"
            << "  stop                  stop policy and hold standing pose\n"
            << "  lay                   smoothly return to laying pose\n"
            << "  obs                   print SDK-order joint positions\n"
            << "  state                 print client/control state\n"
            << "  quit                  disable Low-level, disconnect and exit\n";
    }

    ClientPtr client_;
    TensorRTPolicy& policy_;
    MotorLayout layout_{};
    std::atomic<bool> closed_{false};
    mutable std::mutex stateMutex_;
    mutable std::condition_variable stateChanged_;
    std::atomic<bool> running_{false};
    std::thread controlThread_;
    mutable std::mutex controlMutex_;
    Mode mode_ = Mode::kIdle;
    std::array<float, kMotorCount> holdPose_ = kStandSdk;
    std::array<float, 3> command_{{0.0f, 0.0f, 0.0f}};
    std::array<float, kMotorCount> lastAction_{};
    std::atomic<uint64_t> sent_{0};
    std::atomic<uint64_t> failed_{0};
    std::string posture_ = "laying";
};

}  // namespace

int main(int argc, char** argv) {
    Options options;
    bool help = false;
    if (!parseOptions(argc, argv, options, help)) {
        printUsage(argv[0]);
        return help ? 0 : 2;
    }
    std::signal(SIGINT, onSignal);
    std::signal(SIGTERM, onSignal);

    try {
        TensorRTPolicy policy(options.onnxPath, options.workspaceMiB);
        if (options.validateOnly) {
            std::array<float, 45> zeros{};
            const auto output = policy.infer(zeros);
            const bool finite = std::all_of(output.begin(), output.end(),
                [](float value) { return std::isfinite(value); });
            if (!finite) throw std::runtime_error("validation output contains NaN or Inf");
            std::cout << "[PASS] TensorRT engine built; input=(1,45) output=(1,12) dtype=float32\n";
            return 0;
        }

        auto service = IMotionSdkService::instance();
        if (!service->initialService(nullptr, options.clientId.c_str())) {
            throw std::runtime_error("SDK initialService failed");
        }
        int result = 0;
        try {
            auto client = Client::create();
            if (!client) throw std::runtime_error("create Low-level client failed");
            LowLevelTensorRTCli cli(client, policy);
            cli.connect(options.observedHz, options.leaseMs);
            cli.run();
            cli.close();
        } catch (...) {
            result = 1;
            service->shutdown();
            throw;
        }
        service->shutdown();
        return result;
    } catch (const std::exception& error) {
        std::cerr << "[FAIL] " << error.what() << '\n';
        return 1;
    }
}
