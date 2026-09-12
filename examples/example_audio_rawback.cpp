// 大脑本机共享内存 RawBack：16 kHz / s16le / mono PCM。
#include "uniubi/robot_sdk/MediaBusClient.h"
#include "uniubi/robot_sdk/MotionHighLevelClient.h"
#include <atomic>
#include "uniubi/robot_sdk/MotionSdkService.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <csignal>
#include <thread>
using namespace uniubi::RobotSdk;
namespace { volatile std::sig_atomic_t stopped = 0; void stop(int) { stopped = 1; } }
int main(int argc, char** argv) {
    char* end = nullptr;
    long volume = argc > 2 ? std::strtol(argv[2], &end, 10) : 20;
    if (argc < 2 || argc > 7 || volume < 0 || volume > 100 || (argc > 2 && (!*argv[2] || *end))) {
        std::fprintf(stderr,"Usage: %s INPUT.pcm [volume=20] [host|-] [device_id|-] [interface|-] [capture_channel]\n",argv[0]);return 1;
    }
    auto arg = [&](int n) {return argc > n && std::strcmp(argv[n],"-") ? argv[n] : "";};
    const char* host=arg(3);const char* device=arg(4);const char* iface=arg(5);
    long channel=-1;
    if(argc>6){channel=std::strtol(argv[6],&end,10);if(!*argv[6]||*end||channel<0||channel>2147483647)return 1;}
    bool capturing=false;std::atomic<unsigned> captured{0};
    FILE* input = std::fopen(argv[1],"rb");
    if (!input) {std::perror("open PCM");return 1;}
    std::signal(SIGINT,stop);std::signal(SIGTERM,stop);
    auto service = IMotionSdkService::instance();
    std::shared_ptr<IMotionHighLevelClient> client;
    IMediaBusClient::Ptr media;
    IAudioRawBackStream::Ptr playback;
    auto run = [&]() -> int {
        if(*iface)service->setNetworkInterface(iface);
        if (!service->initialService(nullptr,"audioRawBackExample")) return 1;
        if(service->isMultiDevice() ? (!*host || !*device) : (*host || *device)){std::fprintf(stderr,"Remote mode requires host and device_id; local mode omits both\n");return 1;}
        client = *device ? IMotionHighLevelClient::create(std::string(device)) : IMotionHighLevelClient::create(false);
        if (!client || !client->connect()) return 1;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (client->getState() != IMotionHighLevelClient::kConnected) {
            if (stopped || std::chrono::steady_clock::now() >= deadline) return 1;
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        media = client->createMediaBusClient();
        if (!media || !media->setup(host)) return 1;
        if(channel>=0){capturing=media->startRawAudioFrame(channel,[&](int32_t,const AudioFrame& f){if(f.valid()&&f.size())++captured;});if(!capturing)return 1;}
        playback = media->createAudioRawBack();
        if (!playback || !playback->setup()) {
            std::fprintf(stderr,"RawBack creation failed (%d); check device RawBack service configuration\n",playback ? playback->getLastError() : media->getLastError());
            return 1;
        }
        deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!playback->ready()){if(stopped || std::chrono::steady_clock::now()>=deadline){std::fprintf(stderr,"RawBack ready timeout\n");return 1;}std::this_thread::sleep_for(std::chrono::milliseconds(20));}
        if(!playback->setVolume(static_cast<int32_t>(volume)))return 1;
        uint8_t data[1280];uint64_t sent = 0;int tailFrames = 0;
        deadline = std::chrono::steady_clock::now();
        while (!stopped) {
            size_t size = std::fread(data,1,sizeof(data),input);
            if (!size) {
                if (std::ferror(input) || !sent) return 1;
                if (tailFrames == 2) break;
                ++tailFrames; // 两帧静音跨过默认预缓冲阈值，包括欠载后重新缓冲。
            }
            if (size % 2) {std::fprintf(stderr,"PCM byte count must be even\n");return 1;}
            std::memset(data + size,0,sizeof(data) - size);
            std::this_thread::sleep_until(deadline);
            AudioFrame frame(data,sizeof(data));auto& info = frame.getAudioFrameInfo();
            info.sampleRate = 16000;info.sampleFormat = 16;info.channelCount = 1;
            info.sequence = sent;info.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            auto readyUntil = std::chrono::steady_clock::now() + std::chrono::seconds(2);
            bool ok = playback->write(frame);
            while (!ok && !sent && !stopped && std::chrono::steady_clock::now() < readyUntil) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));ok = playback->write(frame);
            }
            if (!ok) {std::fprintf(stderr,"RawBack write failed\n");return 1;}
            ++sent;
            deadline = std::max(deadline + std::chrono::milliseconds(40),std::chrono::steady_clock::now());
        }
        // 无播放完成确认；只给末尾混音缓存留出时间。
        if (!stopped) std::this_thread::sleep_for(std::chrono::milliseconds(300));
        if(capturing&&!captured){std::fprintf(stderr,"No audio capture frames received\n");return 1;}
        std::printf("Published %llu PCM frames; captured=%u\n",(unsigned long long)sent,captured.load());
        return 0;
    };
    int result = run();
    if (playback) playback->shutdown();
    if (media) {if(capturing)media->stopRawAudioFrame(channel);media->shutdown();}
    if (client) client->disconnect();
    service->shutdown();std::fclose(input);
    return result;
}
