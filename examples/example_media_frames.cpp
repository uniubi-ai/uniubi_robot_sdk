/**
 * ========================================================
 *  @file example_media_frames.cpp
 *  @brief Motion SDK 媒体帧订阅示例：AudioFrame / VideoFrame / EncodedVideoFrame
 *  @note 仅支持 aarch64 板内本地部署；x86_64/i386 不调用 MediaBus client 接口。
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 * ========================================================
 */

#include <algorithm>
#include <mutex>
#include <vector>
#include <climits>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <string>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "uniubi/robot_sdk/Media/FrameInfo.h"
#include "uniubi/robot_sdk/MediaBusClient.h"
#include "uniubi/robot_sdk/MotionHighLevelClient.h"
#include "uniubi/robot_sdk/MotionSdkService.h"

using namespace uniubi::RobotSdk;

namespace {

struct Options {
    const char* configFile = nullptr;
    std::string clientId = "mediaFrameExample";
    std::string deviceId;
    int32_t videoChannel = 0;
    int32_t audioChannel = 0;
    int32_t seconds = 10;
    std::string networkInterface;
    bool pcm4 = false;
    std::string pcmDir;
};

struct Counter {
    std::atomic<uint64_t> frames{0};
    std::atomic<uint64_t> bytes{0};
    std::atomic<uint32_t> dumped{0};
};

struct Stats {
    Counter videoRaw;
    Counter videoEncoded;
    Counter audioRaw;
};

constexpr uint32_t kDumpLimit = 10;
const char* kDumpDir = "/tmp/media_frame_dump";

int32_t parseInt(const char* value, int32_t fallback) {
    if (value == nullptr || *value == '\0') {
        return fallback;
    }
    char* end = nullptr;
    errno = 0;
    long parsed = std::strtol(value, &end, 10);
    return !errno && end != value && *end == '\0' && parsed >= INT32_MIN && parsed <= INT32_MAX ? static_cast<int32_t>(parsed) : fallback;
}

Options parseArgs(int argc, char** argv) {
    Options options;
    if (argc > 1 && (std::string(argv[1]) == "--pcm4" || std::string(argv[1]) == "--capture-all")) {
        options.pcm4 = true;
        if (argc > 5) { options.seconds = 0; return options; }
        options.pcmDir = argc > 2 ? argv[2] : "";
        options.seconds = argc > 3 ? parseInt(argv[3], 0) : 20;
        options.networkInterface = argc > 4 && std::string(argv[4]) != "-" ? argv[4] : "";
        return options;
    }
    if (argc > 1 && std::string(argv[1]) != "-") {
        options.configFile = argv[1];
    }
    if (argc > 2) {
        options.clientId = argv[2];
    }
    if (argc > 3 && std::string(argv[3]) != "-") {
        options.deviceId = argv[3];
    }
    if (argc > 4) {
        options.videoChannel = parseInt(argv[4], options.videoChannel);
    }
    if (argc > 5) {
        options.audioChannel = parseInt(argv[5], options.audioChannel);
    }
    if (argc > 6) {
        options.seconds = parseInt(argv[6], options.seconds);
    }
    if (argc > 7 && std::string(argv[7]) != "-") {
        options.networkInterface = argv[7];
    }
    return options;
}

void printUsage(const char* program) {
    std::printf("usage: %s [config|-] [client_id] [device_id|-] [video_channel] [audio_channel] [seconds] [network_iface|-]\n",
                program);
    std::printf("       %s --pcm4 <new-output-dir> [seconds:1..60, default 20] [network_iface|-]\n", program);
    std::printf("       --capture-all (alias --pcm4): 5 NV21 images per camera and 4 PCM channels\n");
    std::printf("       media frame subscription is supported only on aarch64 local board deployment.\n");
    std::printf("       config configures the Motion SDK service; MediaBus reads /etc/robot/sdk_config.json.\n");
    std::printf("example: %s - mediaFrameExample - 0 0 10 eth0\n", program);
}

const char* videoCodecName(uint8_t codec) {
    switch (codec) {
        case Uface::Media::videoH264: return "H264";
        case Uface::Media::videoH265: return "H265";
        case Uface::Media::videoMpeg4: return "MPEG4";
        case Uface::Media::videoMJpeg: return "MJPEG";
        case Uface::Media::videoJpeg: return "JPEG";
        default: return "unknown";
    }
}

const char* pixelFormatName(Uface::Media::MediaPixelFormat format) {
    switch (format) {
        case Uface::Media::mediaPixelFormatNV12: return "NV12";
        case Uface::Media::mediaPixelFormatNV21: return "NV21";
        case Uface::Media::mediaPixelFormatNV16: return "NV16";
        case Uface::Media::mediaPixelFormatNV61: return "NV61";
        case Uface::Media::mediaPixelFormatRGB888: return "RGB888";
        case Uface::Media::mediaPixelFormatBGR888: return "BGR888";
        case Uface::Media::mediaPixelFormatARGB8888: return "ARGB8888";
        case Uface::Media::mediaPixelFormatABGR8888: return "ABGR8888";
        case Uface::Media::mediaPixelFormatYUYV422: return "YUYV422";
        case Uface::Media::mediaPixelFormatUYVY422: return "UYVY422";
        case Uface::Media::mediaPixelFormatYUV420P: return "YUV420P";
        case Uface::Media::mediaPixelFormatYUV422P: return "YUV422P";
        case Uface::Media::mediaPixelFormatYUV444P: return "YUV444P";
        case Uface::Media::mediaPixelFormatYUV400: return "YUV400";
        case Uface::Media::mediaPixelFormatYUV420M: return "YUV420M";
        case Uface::Media::mediaPixelFormatYUV422RM: return "YUV422RM";
        case Uface::Media::mediaPixelFormatYUV422M: return "YUV422M";
        case Uface::Media::mediaPixelFormatYUV444M: return "YUV444M";
        case Uface::Media::mediaPixelFormatRGBA8888: return "RGBA8888";
        case Uface::Media::mediaPixelFormatBGRA8888: return "BGRA8888";
        case Uface::Media::mediaPixelFormatRGB565: return "RGB565";
        case Uface::Media::mediaPixelFormatRGB888Planar: return "RGB888P";
        case Uface::Media::mediaPixelFormatBGR888Planar: return "BGR888P";
        case Uface::Media::mediaPixelFormatS16C1: return "S16C1";
        case Uface::Media::mediaPixelFormatU16C1: return "U16C1";
        default: return "UNKNOWN";
    }
}

const char* frameTypeName(uint8_t type) {
    switch (type) {
        case Uface::Stream::audioFrame: return "audio";
        case Uface::Stream::videoFrame: return "video";
        case Uface::Stream::auxFrame: return "aux";
        case Uface::Stream::imageFrame: return "image";
        case Uface::Stream::videoBFrame: return "B";
        case Uface::Stream::videoIFrame: return "I";
        case Uface::Stream::videoPFrame: return "P";
        case Uface::Stream::waterFrame: return "water";
        case Uface::Stream::auxMetaFrame: return "meta";
        default: return "unknown";
    }
}

const Uface::Stream::FrameInfo* encodedFrameInfo(const EncodedVideoFrame& frame) {
    return reinterpret_cast<const Uface::Stream::FrameInfo*>(frame.getExtraData());
}

bool ensureDumpDir() {
    if (::mkdir(kDumpDir, 0755) == 0 || errno == EEXIST) {
        return true;
    }

    std::fprintf(stderr, "create dump dir %s failed: %s\n", kDumpDir, std::strerror(errno));
    return false;
}

bool dumpBytes(const char* path, const uint8_t* data, uint32_t size) {
    if (data == nullptr || size == 0) {
        return false;
    }

    FILE* fp = std::fopen(path, "wb");
    if (fp == nullptr) {
        std::fprintf(stderr, "open dump file %s failed: %s\n", path, std::strerror(errno));
        return false;
    }

    const size_t written = std::fwrite(data, 1, size, fp);
    std::fclose(fp);
    if (written != size) {
        std::fprintf(stderr, "write dump file %s failed: %zu/%u\n", path, written, size);
        return false;
    }

    return true;
}

bool writePlaneRows(int32_t fd, const uint8_t* data, uint32_t width, uint32_t height, uint32_t stride) {
    if (data == nullptr || width == 0 || height == 0 || stride == 0) {
        return false;
    }

    for (uint32_t row = 0; row < height; ++row) {
        const uint8_t* line = data + static_cast<size_t>(row) * stride;
        uint32_t remain = width;
        while (remain > 0) {
            ssize_t written = ::write(fd, line, remain);
            if (written <= 0) {
                return false;
            }
            line += written;
            remain -= static_cast<uint32_t>(written);
        }
    }
    return true;
}

const uint8_t* planeAddress(const VideoFrame& frame, const Uface::Media::VideoFrameInfo& info, int32_t plane) {
    if (plane < 0 || plane >= 3) {
        return nullptr;
    }

    const uint8_t* data = info.virAddr[plane];
    if (plane == 0 && data == nullptr) {
        data = frame.data();
    }
    return data;
}

bool writePlane(int32_t fd, const VideoFrame& frame, const Uface::Media::VideoFrameInfo& info,
                int32_t plane, uint32_t width, uint32_t height, uint32_t fallbackStride) {
    if (height == 0) {
        return true;
    }

    const uint8_t* data = planeAddress(frame, info, plane);
    const uint32_t stride = info.stride[plane] > 0 ? info.stride[plane] : fallbackStride;
    if (data == nullptr || stride == 0) {
        std::fprintf(stderr, "skip video raw dump: plane=%d data=%p stride=%u\n",
                     plane, static_cast<const void*>(data), stride);
        return false;
    }

    if (!writePlaneRows(fd, data, width, height, stride)) {
        std::fprintf(stderr, "write video raw plane failed: plane=%d errno=%s\n",
                     plane, std::strerror(errno));
        return false;
    }
    return true;
}

bool dumpVideoFramePayload(const char* path, const VideoFrame& frame) {
    const auto& info = frame.getFrameInfo();
    const int32_t fd = ::open(path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd < 0) {
        std::fprintf(stderr, "open video raw dump failed: %s\n", path);
        return false;
    }

    bool ok = false;
    const uint32_t width = info.width;
    const uint32_t height = info.height;
    const uint32_t semiPlanarStride = info.stride[0] > 0 ? info.stride[0] : width;
    switch (info.pixelFormat) {
        case Uface::Media::mediaPixelFormatNV12:
        case Uface::Media::mediaPixelFormatNV21:
            ok = writePlane(fd, frame, info, 0, width, height, width) &&
                 writePlane(fd, frame, info, 1, width, (height + 1) / 2, semiPlanarStride);
            break;
        case Uface::Media::mediaPixelFormatNV16:
        case Uface::Media::mediaPixelFormatNV61:
            ok = writePlane(fd, frame, info, 0, width, height, width) &&
                 writePlane(fd, frame, info, 1, width, height, semiPlanarStride);
            break;
        case Uface::Media::mediaPixelFormatYUV400:
            ok = writePlane(fd, frame, info, 0, width, height, width);
            break;
        case Uface::Media::mediaPixelFormatYUV420P:
        case Uface::Media::mediaPixelFormatYUV420M:
            ok = writePlane(fd, frame, info, 0, width, height, width) &&
                 writePlane(fd, frame, info, 1, (width + 1) / 2, (height + 1) / 2, (width + 1) / 2) &&
                 writePlane(fd, frame, info, 2, (width + 1) / 2, (height + 1) / 2, (width + 1) / 2);
            break;
        case Uface::Media::mediaPixelFormatYUV422P:
        case Uface::Media::mediaPixelFormatYUV422M:
        case Uface::Media::mediaPixelFormatYUV422RM:
            ok = writePlane(fd, frame, info, 0, width, height, width) &&
                 writePlane(fd, frame, info, 1, (width + 1) / 2, height, (width + 1) / 2) &&
                 writePlane(fd, frame, info, 2, (width + 1) / 2, height, (width + 1) / 2);
            break;
        case Uface::Media::mediaPixelFormatYUV444P:
        case Uface::Media::mediaPixelFormatYUV444M:
        case Uface::Media::mediaPixelFormatRGB888Planar:
        case Uface::Media::mediaPixelFormatBGR888Planar:
            ok = writePlane(fd, frame, info, 0, width, height, width) &&
                 writePlane(fd, frame, info, 1, width, height, width) &&
                 writePlane(fd, frame, info, 2, width, height, width);
            break;
        case Uface::Media::mediaPixelFormatRGB888:
        case Uface::Media::mediaPixelFormatBGR888:
            ok = writePlane(fd, frame, info, 0, width * 3, height, width * 3);
            break;
        case Uface::Media::mediaPixelFormatRGBA8888:
        case Uface::Media::mediaPixelFormatBGRA8888:
        case Uface::Media::mediaPixelFormatARGB8888:
        case Uface::Media::mediaPixelFormatABGR8888:
            ok = writePlane(fd, frame, info, 0, width * 4, height, width * 4);
            break;
        case Uface::Media::mediaPixelFormatRGB565:
        case Uface::Media::mediaPixelFormatYUYV422:
        case Uface::Media::mediaPixelFormatUYVY422:
        case Uface::Media::mediaPixelFormatS16C1:
        case Uface::Media::mediaPixelFormatU16C1:
            ok = writePlane(fd, frame, info, 0, width * 2, height, width * 2);
            break;
        default:
            ::close(fd);
            ok = dumpBytes(path, frame.data(), frame.size());
            return ok;
    }

    ::close(fd);
    return ok;
}

bool shouldDump(Counter& counter, uint32_t& index) {
    index = counter.dumped.fetch_add(1, std::memory_order_acq_rel);
    if (index < kDumpLimit) {
        return true;
    }

    counter.dumped.fetch_sub(1, std::memory_order_acq_rel);
    return false;
}

void dumpVideoFrame(Counter& counter, int32_t channel, const VideoFrame& frame) {
    uint32_t index = 0;
    if (!shouldDump(counter, index)) {
        return;
    }

    const auto& info = frame.getFrameInfo();
    char path[256] = {0};
    std::snprintf(path, sizeof(path), "%s/video_raw_ch%d_%02u_%ux%u_%s_seq%llu.yuv",
                  kDumpDir,
                  channel,
                  index,
                  info.width,
                  info.height,
                  pixelFormatName(info.pixelFormat),
                  static_cast<unsigned long long>(info.sequence));
    if (dumpVideoFramePayload(path, frame)) {
        std::printf("[dump] %s raw_size=%u stride=%u/%u/%u\n",
                    path, frame.size(), info.stride[0], info.stride[1], info.stride[2]);
    }
}

void dumpAudioFrame(Counter& counter, int32_t channel, const AudioFrame& frame) {
    uint32_t index = 0;
    if (!shouldDump(counter, index)) {
        return;
    }

    const auto& info = frame.getFrameInfo();
    char path[256] = {0};
    std::snprintf(path, sizeof(path), "%s/audio_raw_ch%d_%02u_rate%u_bit%u_ch%u_seq%llu.pcm",
                  kDumpDir,
                  channel,
                  index,
                  info.sampleRate,
                  info.sampleFormat,
                  info.channelCount,
                  static_cast<unsigned long long>(info.sequence));
    if (dumpBytes(path, frame.data(), frame.size())) {
        std::printf("[dump] %s size=%u\n", path, frame.size());
    }
}

const char* encodedVideoExt(uint8_t codec) {
    switch (codec) {
        case Uface::Media::videoH264: return "h264";
        case Uface::Media::videoH265: return "h265";
        case Uface::Media::videoMJpeg:
        case Uface::Media::videoJpeg: return "jpg";
        default: return "bin";
    }
}

void dumpEncodedFrame(Counter& counter, int32_t channel, const EncodedVideoFrame& frame) {
    uint32_t index = 0;
    if (!shouldDump(counter, index)) {
        return;
    }

    const auto* info = encodedFrameInfo(frame);
    const uint8_t codec = info != nullptr ? info->detail.video.encode : 0;
    const uint32_t width = info != nullptr ? info->detail.video.width : 0;
    const uint32_t height = info != nullptr ? info->detail.video.height : 0;
    char path[256] = {0};
    std::snprintf(path, sizeof(path), "%s/video_encoded_ch%d_%02u_%ux%u_type0x%02x_seq%d.%s",
                  kDumpDir,
                  channel,
                  index,
                  width,
                  height,
                  frame.getFrameType(),
                  frame.getSequence(),
                  encodedVideoExt(codec));
    if (dumpBytes(path, frame.getBuffer(), static_cast<uint32_t>(frame.size()))) {
        std::printf("[dump] %s size=%d\n", path, frame.size());
    }
}

void onVideoFrame(Stats& stats, int32_t channel, const VideoFrame& frame) {
    const uint64_t count = ++stats.videoRaw.frames;
    stats.videoRaw.bytes += frame.size();
    dumpVideoFrame(stats.videoRaw, channel, frame);

    if (count != 1 && count % 30 != 0) {
        return;
    }

    const auto& info = frame.getFrameInfo();
    std::printf("[video-raw] ch=%d count=%llu size=%u fd=%d %ux%u fmt=%d ts=%llu seq=%llu\n",
                channel,
                static_cast<unsigned long long>(count),
                frame.size(),
                frame.getFd(),
                info.width,
                info.height,
                static_cast<int>(info.pixelFormat),
                static_cast<unsigned long long>(info.timestamp),
                static_cast<unsigned long long>(info.sequence));
}

void onAudioFrame(Stats& stats, int32_t channel, const AudioFrame& frame) {
    const uint64_t count = ++stats.audioRaw.frames;
    stats.audioRaw.bytes += frame.size();
    dumpAudioFrame(stats.audioRaw, channel, frame);

    if (count != 1 && count % 30 != 0) {
        return;
    }

    const auto& info = frame.getFrameInfo();
    std::printf("[audio-raw] ch=%d count=%llu size=%u fd=%d rate=%u bit=%u channels=%u type=%u ts=%llu seq=%llu\n",
                channel,
                static_cast<unsigned long long>(count),
                frame.size(),
                frame.getFd(),
                info.sampleRate,
                info.sampleFormat,
                info.channelCount,
                info.dataType,
                static_cast<unsigned long long>(info.timestamp),
                static_cast<unsigned long long>(info.sequence));
}

void onEncodedFrame(Stats& stats, int32_t channel, const EncodedVideoFrame& frame) {
    const uint64_t count = ++stats.videoEncoded.frames;
    stats.videoEncoded.bytes += frame.size();
    dumpEncodedFrame(stats.videoEncoded, channel, frame);

    if (count != 1 && count % 30 != 0) {
        return;
    }

    const auto* info = encodedFrameInfo(frame);
    if (info == nullptr) {
        std::printf("[video-encoded] ch=%d count=%llu size=%d no frame info\n",
                    channel, static_cast<unsigned long long>(count), frame.size());
        return;
    }

    const auto& video = info->detail.video;
    std::printf("[video-encoded] ch=%d count=%llu size=%d codec=%s(%u) type=%s(0x%02x) "
                "%ux%u fps=%u/%u pts=%llu seq=%d utc=%llu change=%u\n",
                channel,
                static_cast<unsigned long long>(count),
                frame.size(),
                videoCodecName(video.encode),
                video.encode,
                frameTypeName(frame.getFrameType()),
                frame.getFrameType(),
                video.width,
                video.height,
                video.fpsNum,
                video.fpsDen,
                static_cast<unsigned long long>(frame.getPts()),
                frame.getSequence(),
                static_cast<unsigned long long>(frame.getUtc()),
                info->change);
}

void printSummary(const Stats& stats) {
    std::printf("[summary] video_raw frames=%llu bytes=%llu\n",
                static_cast<unsigned long long>(stats.videoRaw.frames.load()),
                static_cast<unsigned long long>(stats.videoRaw.bytes.load()));
    std::printf("[summary] video_encoded frames=%llu bytes=%llu\n",
                static_cast<unsigned long long>(stats.videoEncoded.frames.load()),
                static_cast<unsigned long long>(stats.videoEncoded.bytes.load()));
    std::printf("[summary] audio_raw frames=%llu bytes=%llu\n",
                static_cast<unsigned long long>(stats.audioRaw.frames.load()),
                static_cast<unsigned long long>(stats.audioRaw.bytes.load()));
    std::printf("[summary] dumped files: video_raw=%u video_encoded=%u audio_raw=%u dir=%s\n",
                stats.videoRaw.dumped.load(),
                stats.videoEncoded.dumped.load(),
                stats.audioRaw.dumped.load(),
                kDumpDir);
}

// Bounded copies in callbacks; save two NV21 cameras and four PCM streams after shutdown.
bool capturePcm4(const IMediaBusClient::Ptr& media, const Options& options) {
    const size_t targetBytes = static_cast<size_t>(options.seconds) * 16000 * sizeof(int16_t);
    std::vector<uint8_t> pcm[4];
    uint64_t frames[4] = {}, invalid[4] = {}, duplicates[4] = {}, lastPts[4] = {};
    bool subscribed[4] = {};
    bool accepting = false;
    std::mutex mutex;
    std::vector<std::vector<uint8_t>> images[2];
    std::vector<std::string> imageNames[2];
    uint64_t videoFrames[2] = {}, videoInvalid[2] = {};
    bool videoSubscribed[2] = {};
    for (auto& buffer : pcm) buffer.reserve(targetBytes);
    bool ready = true;
    for (int32_t ch = 0; ch < 4; ++ch) {
        subscribed[ch] = media->startRawAudioFrame(ch, [&, ch](int32_t channel, const AudioFrame& frame) {
            std::lock_guard<std::mutex> guard(mutex);
            if (!accepting || pcm[ch].size() >= targetBytes) return;
            const auto& info = frame.getFrameInfo();
            if (channel != ch || !frame.data() || !frame.size() || frame.size() % 2 ||
                info.dataType != 0 || info.sampleRate != 16000 || info.sampleFormat != 16 || info.channelCount != 1) {
                ++invalid[ch];
                return;
            }
            if (frames[ch] && info.timestamp <= lastPts[ch]) {
                ++duplicates[ch];
                return;
            }
            lastPts[ch] = info.timestamp;
            const size_t bytes = std::min(static_cast<size_t>(frame.size()), targetBytes - pcm[ch].size());
            pcm[ch].insert(pcm[ch].end(), frame.data(), frame.data() + bytes);
            ++frames[ch];
        });
        std::printf("[pcm4-subscribe] ch=%d ok=%d error=%d\n", ch, subscribed[ch], media->getLastError());
        ready = ready && subscribed[ch];
    }
    for (int32_t ch = 0; ch < 2; ++ch) {
        videoSubscribed[ch] = media->startRawVideoFrame(ch, [&, ch](int32_t channel, const VideoFrame& frame) {
            std::lock_guard<std::mutex> guard(mutex);
            if (!accepting || images[ch].size() >= 5) return;
            ++videoFrames[ch];
            const auto& info = frame.getFrameInfo();
            if (channel != ch || info.pixelFormat != Uface::Media::mediaPixelFormatNV21 ||
                !info.width || !info.height || info.width % 2 || info.height % 2 ||
                info.width > 4096 || info.height > 2160 || !info.virAddr[0] || !info.virAddr[1] ||
                info.stride[0] < info.width || info.stride[1] < info.width) {
                ++videoInvalid[ch];
                return;
            }
            // 只复制前5帧，去掉stride填充，不长期占用服务端图像缓冲区。
            if (images[ch].size() < 5) {
                std::vector<uint8_t> image(static_cast<size_t>(info.width) * info.height * 3 / 2);
                for (uint32_t row = 0; row < info.height; ++row)
                    std::memcpy(image.data() + static_cast<size_t>(row) * info.width,
                                info.virAddr[0] + static_cast<size_t>(row) * info.stride[0], info.width);
                for (uint32_t row = 0; row < info.height / 2; ++row)
                    std::memcpy(image.data() + static_cast<size_t>(info.width) * info.height + static_cast<size_t>(row) * info.width,
                                info.virAddr[1] + static_cast<size_t>(row) * info.stride[1], info.width);
                imageNames[ch].push_back("/camera" + std::to_string(ch) + "_" + std::to_string(info.width) + "x" +
                    std::to_string(info.height) + "_pts" + std::to_string(info.timestamp) + "_" + std::to_string(videoFrames[ch]) + ".nv21");
                images[ch].push_back(std::move(image));
            }
        });
        std::printf("[video-subscribe] ch=%d ok=%d error=%d\n", ch, videoSubscribed[ch], media->getLastError());
        ready = ready && videoSubscribed[ch];
    }
    if (ready) {
        {
            std::lock_guard<std::mutex> guard(mutex);
            accepting = true;
        }
        std::printf("[pcm4-recording] SPEAK NOW: %d seconds, output=%s\n", options.seconds, options.pcmDir.c_str());
        std::fflush(stdout);
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(options.seconds + 5);
        const auto minimumEnd = std::chrono::steady_clock::now() + std::chrono::seconds(options.seconds);
        while (std::chrono::steady_clock::now() < deadline) {
            bool complete = true;
            {
                std::lock_guard<std::mutex> guard(mutex);
                for (const auto& buffer : pcm) complete = complete && buffer.size() == targetBytes;
                for (const auto& frames : images) complete = complete && frames.size() == 5;
            }
            if (complete && std::chrono::steady_clock::now() >= minimumEnd) break;
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    {
        std::lock_guard<std::mutex> guard(mutex);
        accepting = false;
    }
    for (int32_t ch = 0; ch < 4; ++ch) if (subscribed[ch]) media->stopRawAudioFrame(ch);
    for (int32_t ch = 0; ch < 2; ++ch) if (videoSubscribed[ch]) media->stopRawVideoFrame(ch);
    media->shutdown(); // 注销并停止回调，局部缓冲区随后才可销毁。
    bool success = ready;
    for (int32_t ch = 0; ch < 4; ++ch) {
        const std::string path = options.pcmDir + "/dmic_ch" + std::to_string(ch) + ".pcm";
        FILE* fp = std::fopen(path.c_str(), "wb");
        bool saved = false;
        if (fp) {
            saved = std::fwrite(pcm[ch].data(), 1, pcm[ch].size(), fp) == pcm[ch].size();
            saved = std::fclose(fp) == 0 && saved;
        }
        uint64_t nonzero = 0;
        for (size_t i = 0; i + 1 < pcm[ch].size(); i += 2) {
            if (pcm[ch][i] || pcm[ch][i + 1]) ++nonzero;
        }
        std::printf("[pcm4-summary] ch=%d frames=%llu bytes=%zu seconds=%.3f invalid=%llu duplicates=%llu nonzero=%llu saved=%d path=%s\n",
                    ch, static_cast<unsigned long long>(frames[ch]), pcm[ch].size(), pcm[ch].size() / 32000.0,
                    static_cast<unsigned long long>(invalid[ch]), static_cast<unsigned long long>(duplicates[ch]),
                    static_cast<unsigned long long>(nonzero), saved, path.c_str());
        // 非零样本数仅用于诊断；已知硬件静音不影响接收和保存验收。
        success = success && saved && pcm[ch].size() == targetBytes && invalid[ch] == 0 && duplicates[ch] == 0;
    }
    for (int32_t ch = 0; ch < 2; ++ch) {
        bool saved = true;
        for (size_t i = 0; i < images[ch].size(); ++i) {
            const std::string path = options.pcmDir + imageNames[ch][i];
            FILE* fp = std::fopen(path.c_str(), "wb");
            if (!fp) { saved = false; continue; }
            saved = (std::fwrite(images[ch][i].data(), 1, images[ch][i].size(), fp) == images[ch][i].size()) && saved;
            saved = (std::fclose(fp) == 0) && saved;
        }
        std::printf("[video-summary] ch=%d frames=%llu nv21=%zu invalid=%llu saved=%d\n",
            ch, static_cast<unsigned long long>(videoFrames[ch]), images[ch].size(),
            static_cast<unsigned long long>(videoInvalid[ch]), saved);
        success = success && saved && images[ch].size() == 5 && !videoInvalid[ch];
    }
    return success;
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        printUsage(argv[0]);
        return 0;
    }

    const Options options = parseArgs(argc, argv);
    printUsage(argv[0]);
#if !defined(__aarch64__)
    std::fprintf(stderr, "media frames require an aarch64 local board\n");
    return 1;
#endif
    if (options.pcm4) {
        if (options.pcmDir.empty() || options.seconds < 1 || options.seconds > 60 ||
            ::mkdir(options.pcmDir.c_str(), 0755) != 0) {
            std::fprintf(stderr, "capture requires 1..60 seconds and a new output directory\n");
            return 1;
        }
    } else if (!ensureDumpDir()) return 1;

    auto* service = IMotionSdkService::instance();
    if (!options.networkInterface.empty()) {
        service->setNetworkInterface(options.networkInterface.c_str());
    }

    service->setLogCallback([](IMotionSdkService::LogLevel level, const char* msg, int32_t length) {
        std::printf("[sdk-log][%d] %.*s\n", static_cast<int>(level), length, msg);
    });

    if (!service->initialService(options.configFile, options.clientId.c_str())) {
        std::fprintf(stderr, "SDK initialService failed\n");
        return 1;
    }

    if (service->isMultiDevice() || !options.deviceId.empty()) {
        std::fprintf(stderr, "media frames require local deployment without device_id\n");
        service->shutdown();
        return 1;
    }

    auto client = options.deviceId.empty()
                      ? IMotionHighLevelClient::create(/*asMaster=*/false)
                      : IMotionHighLevelClient::create(options.deviceId);
    if (!client) {
        std::fprintf(stderr, "create high level client failed\n");
        service->shutdown();
        return 1;
    }

    if (!client->connect()) {
        std::fprintf(stderr, "connect failed: %d\n", static_cast<int>(client->getLastError()));
        service->shutdown();
        return 1;
    }

    const auto connectDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (client->getState() != IMotionHighLevelClient::kConnected) {
        if (std::chrono::steady_clock::now() > connectDeadline) {
            std::fprintf(stderr, "wait connected timeout\n");
            client->disconnect();
            service->shutdown();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    /// 音视频通道统一走 IMediaBusClient（与控制权无关，setup 后即可订阅）
    auto media = client->createMediaBusClient();
    if (!media || !media->setup()) {
        std::fprintf(stderr, "create/setup media bus client failed\n");
        service->shutdown();
        return 1;
    }

    MediaLayout layout = {};
    if (media->getMediaLayout(layout)) {
        std::printf("media layout: mic=%u camera=%u encoder=%u\n",
                    layout.micNum, layout.cameraNum, layout.videoEncoderNum);
    }

    if (options.pcm4) {
        const bool success = capturePcm4(media, options);
        client->disconnect();
        service->shutdown();
        return success ? 0 : 2;
    }
    Stats stats;
    const bool videoRawOk = media->startRawVideoFrame(
        options.videoChannel,
        [&stats](int32_t channel, const VideoFrame& frame) {
            onVideoFrame(stats, channel, frame);
        });
    const bool encodedOk = media->startEncodedVideoFrame(
        options.videoChannel,
        [&stats](int32_t channel, const EncodedVideoFrame& frame) {
            onEncodedFrame(stats, channel, frame);
        });
    const bool audioOk = media->startRawAudioFrame(
        options.audioChannel,
        [&stats](int32_t channel, const AudioFrame& frame) {
            onAudioFrame(stats, channel, frame);
        });

    std::printf("subscribe result: video_raw=%d video_encoded=%d audio_raw=%d\n",
                videoRawOk ? 1 : 0, encodedOk ? 1 : 0, audioOk ? 1 : 0);

    for (int32_t i = 0; i < options.seconds; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::printf("[tick] %d/%d\n", i + 1, options.seconds);
    }

    media->stopEncodedVideoFrame(options.videoChannel);
    media->stopRawVideoFrame(options.videoChannel);
    media->stopRawAudioFrame(options.audioChannel);
    media->shutdown();

    printSummary(stats);
    client->disconnect();
    service->shutdown();
    return 0;
}
