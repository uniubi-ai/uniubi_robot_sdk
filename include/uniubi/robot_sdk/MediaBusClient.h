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
#include <cstdint>
#include <functional>
#include "uniubi/robot_sdk/Media/MediaFrame.h"
#include "uniubi/robot_sdk/Media/MediaBuffer.h"
#include "uniubi/robot_sdk/MotionSdkProtocol.h"

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
    } MediaBusError;

    using Ptr = std::shared_ptr<IMediaBusClient>;
    using RawVideoFrameCallback = std::function<void(int32_t channel, const VideoFrame& frame)>;
    using RawAudioFrameCallback = std::function<void(int32_t channel, const AudioFrame& frame)>;
    using EncodedVideoFrameCallback = std::function<void(int32_t channel,const EncodedVideoFrame& frame)>;
public:
    /**
     * @brief 开启media bus
     * @return
     */
    virtual bool setup() = 0;
    /**
     * @brief 关闭media bus
     */
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
     * @brief 停止订阅音频原始帧
     */
    virtual void stopRawAudioFrame(int32_t channel) = 0;
    /**
     * @brief 获取音视频能力
     * @param layout
     * @return
     */
    virtual bool getMediaLayout(MediaLayout& layout) = 0;
    /**
     * @brief 停止订阅视频编码帧
     */
    virtual void stopEncodedVideoFrame(int32_t channel) = 0;
    /**
     * @brief 订阅视频原始帧，frame 使用 MediaBus VideoFrame 格式
     * @param channel 视频输入通道
     * @param cb 帧回调；为空返回 false
     */
    virtual bool startRawVideoFrame(int32_t channel, RawVideoFrameCallback cb) = 0;
    /**
     * @brief 订阅音频原始帧，frame 使用 MediaBus AudioFrame 格式
     * @param channel 音频输入通道
     * @param cb 帧回调；为空返回 false
     */
    virtual bool startRawAudioFrame(int32_t channel, RawAudioFrameCallback cb) = 0;
    /**
     * @brief 订阅视频编码帧，frame 使用 EncodedVideoFrame 格式
     * @param channel 视频源通道
     * @param cb 编码帧回调；为空返回 false
     */
    virtual bool startEncodedVideoFrame(int32_t channel, EncodedVideoFrameCallback cb) = 0;
};
}
}
#endif //ROBOTSERVICE_MEDIABUSCLIENT_H
