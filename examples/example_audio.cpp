// 大脑本机共享内存 PCM 采集；与视频使用同一个 IMediaBusClient。
#include "uniubi/robot_sdk/MediaBusClient.h"
#include "uniubi/robot_sdk/MotionLowLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <mutex>
#include <thread>
#include <vector>

using namespace uniubi::RobotSdk;
namespace {
volatile std::sig_atomic_t stopped = 0;
void stop(int) { stopped = 1; }
bool integer(const char* text, int& value, int minimum) {
    char* end = nullptr;
    const long parsed = std::strtol(text, &end, 10);
    if (!*text || *end || parsed < minimum || parsed > 86400) return false;
    value = static_cast<int>(parsed);
    return true;
}
}

int main(int argc, char** argv) {
    int channel = 0, seconds = 10;
    if (argc > 4 || (argc > 1 && !integer(argv[1], channel, 0)) ||
        (argc > 2 && !integer(argv[2], seconds, 1))) {
        std::fprintf(stderr, "Usage: %s [channel=0] [seconds=10] [output.pcm]\n", argv[0]);
        return 1;
    }
    std::signal(SIGINT, stop);
    std::signal(SIGTERM, stop);
    auto service = IMotionSdkService::instance();
    if (!service->initialService(nullptr, "audioShmExample")) return 1;
    if (service->isMultiDevice()) {
        std::fprintf(stderr, "Run on the onboard brain; MediaBus uses /etc/robot/sdk_config.json.\n");
        service->shutdown();
        return 1;
    }
    auto client = IMotionLowLevelClient::create();
    IMediaBusClient::Ptr media;
    FILE* output = nullptr;
    bool subscribed = false;
    std::mutex mutex;
    uint64_t frames = 0, bytes = 0, dropped = 0;
    Uface::Media::AudioFrameInfo latest = {};
    std::deque<std::vector<uint8_t>> pending;
    auto run = [&]() -> int {
        // 仅连接以取得媒体工厂；无需使能运动或取得控制权。
        if (!client->connect()) return 1;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (client->getState() != IMotionLowLevelClient::kConnected) {
            if (stopped || std::chrono::steady_clock::now() >= deadline) return 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        media = client->createMediaBusClient();
        if (!media || !media->setup()) {
            std::fprintf(stderr, "MediaBus setup failed, error=%d; check /etc/robot/sdk_config.json\n",
                         media ? media->getLastError() : -1);
            return 1;
        }
        MediaLayout layout = {};
        if (!media->getMediaLayout(layout) || channel >= static_cast<int>(layout.micNum)) {
            std::fprintf(stderr, "Audio channel %d is not configured\n", channel);
            return 1;
        }
        if (argc == 4) {
            output = std::fopen(argv[3], "wb");
            if (!output) { std::perror("open PCM output"); return 1; }
        }
        subscribed = media->startRawAudioFrame(channel, [&](int32_t, const AudioFrame& frame) {
            if (!frame.valid() || !frame.data() || !frame.size()) return;
            std::lock_guard<std::mutex> guard(mutex);
            ++frames;bytes += frame.size();latest = frame.getFrameInfo();
            if (output) {
                if (pending.size() < 64) pending.emplace_back(frame.data(), frame.data() + frame.size());
                else ++dropped;
            }
        });
        if (!subscribed) {
            std::fprintf(stderr, "Audio subscription failed, error=%d\n", media->getLastError());
            return 1;
        }
        auto flush = [&]() {
            std::deque<std::vector<uint8_t>> batch;
            { std::lock_guard<std::mutex> guard(mutex); batch.swap(pending); }
            for (const auto& data : batch)
                if (std::fwrite(data.data(), 1, data.size(), output) != data.size()) return false;
            return true;
        };
        const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        auto report = std::chrono::steady_clock::now() + std::chrono::seconds(1);
        bool writeOk = true;
        while (!stopped && std::chrono::steady_clock::now() < end) {
            if (output && !(writeOk = flush())) break;
            if (std::chrono::steady_clock::now() >= report) {
                std::lock_guard<std::mutex> guard(mutex);
                std::printf("shm audio ch=%d frames=%llu bytes=%llu rate=%u bits=%u channels=%u ts=%llu dropped=%llu\n",
                    channel, (unsigned long long)frames, (unsigned long long)bytes,
                    latest.sampleRate, latest.sampleFormat, latest.channelCount,
                    (unsigned long long)latest.timestamp, (unsigned long long)dropped);
                report += std::chrono::seconds(1);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        media->stopRawAudioFrame(channel);subscribed = false;
        if (output && !flush()) writeOk = false;
        if (!frames) std::fprintf(stderr, "No PCM frames received: check MediaServer input and aiStream channel mapping.\n");
        if (!writeOk || dropped) std::fprintf(stderr, "PCM output incomplete: write failure or queue overflow.\n");
        return !frames || !writeOk || dropped ? 2 : 0;
    };
    int result = run();
    if (media) {
        if (subscribed) media->stopRawAudioFrame(channel);
        media->shutdown();
    }
    if (output && std::fclose(output) != 0) result = 2;
    client->disconnect();
    service->shutdown();
    return result;
}
