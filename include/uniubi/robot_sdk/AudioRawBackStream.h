/**
 * ========================================================
 *  @file AudioRawBackStream.h
 *  @brief
 *  @author shangyang
 *  @date 2026-09-12 15:05
 *  @version 1.0
 *  @details
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 *  @license
 * ========================================================
 */

#ifndef ROBOTSERVICE_AUDIORAWBACKSTREAM_H
#define ROBOTSERVICE_AUDIORAWBACKSTREAM_H

#include <memory>
#include <cstdint>
#include "uniubi/robot_sdk/Media/MediaBuffer.h"

namespace uniubi {
namespace RobotSdk {

/**
 * @brief RawBack 音频发送流；由 IMediaBusClient 创建。
 * @details Orin 部署使用本机共享内存，客户板部署使用网络发送到 DV500。
 * @note 固定接收 16000 Hz、16 bit、单声道 PCM；不执行重采样。
 *       不要在媒体回调中调用流控制方法；析构会调用 shutdown。
 */
class IAudioRawBackStream {

public:

    typedef enum {
        kNone,                  ///< 操作成功。
        kInvalidParam,          ///< 参数无效。
        kAlreadyInitialized,    ///< 已初始化，需先 shutdown。
        kNotInitialized,        ///< 尚未初始化。
        kConnectFailed,         ///< 连接启动失败。
        kNotConnected,          ///< 连接尚未就绪。
        kRpcCallFailed,         ///< RPC 失败，含超时、断线、远端拒绝或无效响应
        kPlaybackFailed,        ///< 通道创建、发送或控制失败
    } AudioError;

    virtual ~IAudioRawBackStream() = default;
    using Ptr = std::shared_ptr<IAudioRawBackStream>;
public:
    /**
     * @brief 请求清空播放缓存，保留播放流。
     * @return true 远端操作成功；false 流不可用或 RPC 失败。
     * @note 同步操作，重连后可能先重建流；不等价于等待已提交数据播放完成。
     */
    virtual bool reset() = 0;
    /**
     * @brief 初始化发送后端。
     * @return true 后端启动成功；网络模式不代表连接已经就绪。
     * @note 错误码通过 getLastError 查询。
    */
    virtual bool setup() = 0;
    /**
     * @brief 关闭发送流，可重复调用；之后可再次 setup。
     */
    virtual void shutdown() = 0;
    /**
     * @brief 查询当前是否可以发送音频数据。
     * @return 当前就绪状态，仅为瞬时快照，不保证随后请求一定成功。
     */
    virtual bool ready() const = 0;
    /**
     * @brief 获取最近一次操作的错误码（AudioError）。
     */
    virtual int32_t getLastError() const = 0;
    /**
     * @brief 设置当前播放流音量。
     * @param volume 音量值，按服务端支持范围设置。
     * @return true 操作成功；false 流不可用或控制失败。
     * @note 不修改硬件总音量；网络重连后会恢复最后设置的流音量。
     */
    virtual bool setVolume(int32_t volume) = 0;
    /**
     * @brief 发送一帧 PCM 数据。
     * @param frame 16000 Hz、16 bit、单声道 PCM 数据及微秒时间戳。
     * @return true 数据已提交，不代表播放完成；false 提交失败。
     * @note 调用者负责发送节奏；接口不缓存失败帧，也不提供播放完成确认。
     */
    virtual bool write(const Uface::Media::AudioFrame &frame) = 0;
};
}
}
#endif //ROBOTSERVICE_AUDIORAWBACKSTREAM_H
