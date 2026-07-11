/************************************************
 * Copyright(c) 2020 uni-ubi
 * 
 * Project:    StreamApp
 * FileName:   MediaFrame.h
 * Author:     shangyang
 * Email:      shangyang@uni-ubi.com
 * Version:    V1.0.0
 * Date:       2022-09-26 23:49
 * Description: 
 * Others:
 *************************************************/

#ifndef _DEPEND_INCLUDE_STREAM_MEDIAFRAME_H_
#define _DEPEND_INCLUDE_STREAM_MEDIAFRAME_H_

#include "uniubi/robot_sdk/Memory/Packet.h"
#include "uniubi/robot_sdk/Media/FrameInfo.h"

namespace Uface {
namespace Stream {

class EXPORT_API CMediaFrame : public Memory::Packet {

public:
    /**
     * @brief 构造函数
     */
    CMediaFrame();
    /**
     * @brief 析构函数
     */
    virtual ~CMediaFrame();
    /**
     * @brief 构造函数;构造一个空包
     * @param[in] bytes  存放的数据大小
     */
    explicit CMediaFrame(uint32_t bytes);
    /**
     * @brief 构造函数
     * @param data
     * @param bytes
     * @param func
     */
    CMediaFrame(uint8_t* data,uint32_t bytes,Memory::FreeMemoryFunc func = Memory::FreeMemoryFunc());
public:
    /**
     * @brief 获取帧通道号
     * @return
     */
    int32_t getChannel() const;
    /**
     * @brief 设置帧通道号
     * @param[in] channel 通道号
     */
    CMediaFrame &setChannel(int channel);
    /**
     * @brief 获取帧类型
     * @return 类型 'A'/'V'/'J'/'X'
     */
    uint8_t getType() const;
    /**
     * @brief 设置类型
     * @param[in] type 类型 'A'/'V'/'J'/'X'
     * @return
     */
    CMediaFrame &setType(uint8_t type);
    /**
     * @brief 或者帧类型
     * @return 帧类型 'A'/'J'/'I'/'P'/'B'/'W'/'M'
     */
    uint8_t getFrameType() const;
    /**
     * @brief 设置帧类型 'A'/'J'/'I'/'P'/'B'/'W'/'M'
     * @note 设置帧类型会来带数据类型一起设置了；数据包类型包括：'A'/'V'/'J'/'X'
     * @param type
     */
    void setFrameType(uint8_t type);
    /**
     * @brief 获取时间戳
     * @return 编码出来的pts
     */
    uint64_t getPts() const;
    /**
     * @brief 设置时间戳
     * @param[in] pts 直接将编码出来的pts设置进去
     * @return
     */
    CMediaFrame &setPts(uint64_t pts);
    /**
     * @brief 获取时间戳
     * @return 编码pts映射到系统的utc时间戳
     */
    uint64_t getUtc() const;
    /**
     * @brief 设置时间戳
     * @param[in] utc 编码pts映射到系统的utc时间戳
     */
    CMediaFrame &setUtc(uint64_t utc);
    /**
     * @brief 获取帧序号
     * @return
     */
    int32_t getSequence() const;
    /**
     * @brief 设置帧序号
     * @param[in] sequence 帧序号，视频和音频的序号单独计算
     */
    CMediaFrame &setSequence(int32_t sequence);
    /**
     * @brief 获取帧信息
     * @return
     */
    FrameInfo* getFrameInfo();
    /**
     * @brief 设置数据信息
     * @param info
     * @return
     */
    CMediaFrame& setFrameInfo(const FrameInfo* info);
    /**
     * @brief 设置视频编码类型，只有I帧才调用这个函数，P帧B帧可以不用调用
     * @param[in] frameInfo 视频帧信息
     */
    CMediaFrame &setVideoInfo(const VFrameInfo &frameInfo);
    /**
     * @brief 设置视频编码类型，只有I帧才调用这个函数，P帧B帧可以不用调用
     * @param[out] frameInfo 视频帧信息
     * @return
     */
    bool getVideoInfo(VFrameInfo &frameInfo);
    /**
     * @brief 设置音频编码类型
     * @param[in] frameInfo 音频帧信息
     */
    CMediaFrame &setAudioInfo(const AFrameInfo &frameInfo);
    /**
     * @brief 设置音频编码类型
     * @param[out] frameInfo 音频帧信息
     * @return
     */
    bool getAudioInfo(AFrameInfo &frameInfo);
    /**
     * @brief 获取帧格式信息变化情况
     * @return @see Stream::StreamChange
     */
    int32_t getNewFormat() const;
    /**
     * @brief 设置编码格式变化情况;
     * @param format @see Stream::StreamChange
     */
    void setNewFormat(int32_t format);
};

}
}
#endif //_DEPEND_INCLUDE_STREAM_MEDIAFRAME_H_
