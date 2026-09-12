/**
 * ========================================================
 *  @file MediaBusClient.h
 *  @brief 
 *  @author shangyang
 *  @date 2026-06-25 13:48
 *  @version 1.0
 *  @details 
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 *  @license 
 * ========================================================
 */

#ifndef ROBOTSERVICE_MEDIABUSCLIENT_H
#define ROBOTSERVICE_MEDIABUSCLIENT_H


#include <memory>
#include <string>
#include <cstdint>
#include <functional>
#include "uniubi/robot_sdk/Media/MediaFrame.h"
#include "uniubi/robot_sdk/Media/MediaBuffer.h"
#include "uniubi/robot_sdk/MotionSdkProtocol.h"
#include "uniubi/robot_sdk/AudioRawBackStream.h"

namespace uniubi {
namespace RobotSdk {

using VideoFrame = Uface::Media::VideoFrame;
using AudioFrame = Uface::Media::AudioFrame;
using EncodedVideoFrame = Uface::Stream::CMediaFrame;
class IMediaBusClient {

public:
    virtual ~IMediaBusClient() = default;

    typedef enum {
        kNone = 0,
        kNotSetup,            ///< 未启动（未 setup 即调用订阅 / 查询接口）
        kConfigLoadFailed,    ///< 加载媒体配置失败
        kConfigInvalid,       ///< 媒体配置缺少 streamDefine 等必填项
        kMediaInitFailed,     ///< 初始化媒体流服务失败
        kMediaStartFailed,    ///< 启动媒体流服务失败
        kInvalidChannel,      ///< 通道号非法（< 0 或 >= 对应硬件数量）
        kInvalidCallback,     ///< 帧回调为空
        kSourceUnavailable,   ///< 编码源不可用（创建失败 / 无视频轨）
        kSourceStartFailed,   ///< 编码源启动失败
        kInvalidParam,        ///< 参数无效或不支持的播放格式
        kCaptureFailed,       ///< 采集订阅登记失败
        kConnectFailed,       ///< 远端媒体服务连接启动失败
        kNotSupported,        ///< 当前部署方式不支持该媒体类型
    } MediaBusError;

    using Ptr = std::shared_ptr<IMediaBusClient>;
    using RawVideoFrameCallback = std::function<void(int32_t channel, const VideoFrame& frame)>;
    using RawAudioFrameCallback = std::function<void(int32_t channel, const AudioFrame& frame)>;
    using EncodedVideoFrameCallback = std::function<void(int32_t channel,const EncodedVideoFrame& frame)>;
public:
    /** @brief 关闭媒体总线、停止全部订阅并关闭已创建的 RawBack 流。 */
    virtual void shutdown() = 0;
    /**
     * @brief 获取最后一次失败原因（MediaBusError）
     */
    virtual int32_t getLastError() const = 0;
    /**
     * @brief 停止订阅视频原始帧
     */
    virtual void stopRawVideoFrame(int32_t channel) = 0;
    /**
     * @brief 停止订阅音频原始帧，等待已开始的回调结束。
     * @note 在业务控制线程调用，不得在媒体回调中调用。
     */
    virtual void stopRawAudioFrame(int32_t channel) = 0;
    /**
     * @brief 获取本机音视频能力。
     * @return Orin 本地模式返回能力；客户板网络模式不支持查询并返回 false。
     */
    virtual bool getMediaLayout(MediaLayout& layout) = 0;
    /**
     * @brief 初始化媒体总线。
     * @param host Orin 本地模式不使用；客户板网络模式必须指定 DV500 地址。
     * @return true 表示后端已启动；网络模式会在后台连接并自动重连。
     */
    virtual bool setup(std::string host = std::string()) = 0;
    /**
     * @brief 创建与当前部署后端匹配的 RawBack 音频流。
     * @return 流对象；MediaBus 尚未 setup 时返回 nullptr。
     * @note 每个 MediaBusClient 内部只维护一个实例；重复调用返回同一对象。
     *       返回后需调用 IAudioRawBackStream::setup()。
     */
    virtual IAudioRawBackStream::Ptr createAudioRawBack() = 0;
    /**
     * @brief 停止订阅视频编码帧
     */
    virtual void stopEncodedVideoFrame(int32_t channel) = 0;
    /**
     * @brief 订阅视频原始帧，frame 使用 MediaBus VideoFrame 格式。
     * @param channel 视频输入通道
     * @param cb 帧回调；为空返回 false。
     * @note 仅 Orin 本地模式支持，客户板网络模式返回 false/kNotSupported。
     */
    virtual bool startRawVideoFrame(int32_t channel, RawVideoFrameCallback cb) = 0;
    /**
     * @brief 订阅 PCM 音频帧。
     * @param channel Orin 模式为 streamDefine.aiStream 下标；网络模式为 DV500 音频 source。
     * @param cb 帧回调；为空返回 false。复制 AudioFrame 可保留帧数据和 metadata。
     * @return true 本地订阅已登记，不代表 MediaServer 已产生音频；需检查回调是否持续到达。
     * @note Orin 模式使用共享内存，客户板模式通过网络从 DV500 获取。
     *       回调运行在媒体接收线程，不得阻塞、抛异常或调用媒体控制接口。
     */
    virtual bool startRawAudioFrame(int32_t channel, RawAudioFrameCallback cb) = 0;
    /**
     * @brief 订阅视频编码帧，frame 使用 EncodedVideoFrame 格式。
     * @param channel 视频源通道
     * @param cb 编码帧回调；为空返回 false。
     * @note 仅 Orin 本地模式支持，客户板网络模式返回 false/kNotSupported。
     */
    virtual bool startEncodedVideoFrame(int32_t channel, EncodedVideoFrameCallback cb) = 0;
};
}
}
#endif //ROBOTSERVICE_MEDIABUSCLIENT_H
