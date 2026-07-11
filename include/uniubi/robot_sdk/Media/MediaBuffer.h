/************************************************
 * Copyright(c) 2020 uni-ubi
 *
 * Project:    Media
 * FileName:   MediaBuffer.h
 * Author:     tianfu
 * Email:      tianfu@uni-ubi.com
 * Version:    V1.0.0
 * Date:       2020-09-09 10:11
 * Description:
 * Others:
 *************************************************/

#ifndef __UFACE_MEDIA_MEDIA_BUFFER_H__
#define __UFACE_MEDIA_MEDIA_BUFFER_H__

#include "uniubi/robot_sdk/Media/Define.h"
#include "uniubi/robot_sdk/UBase/Delegate.h"

namespace Uface {
namespace Media {

/**
 * @brief 图像像素格式
 */
typedef enum {
    mediaPixelFormatUnknown,
    mediaPixelFormatNV12,
    mediaPixelFormatNV21,
    mediaPixelFormatNV16,
    mediaPixelFormatNV61,

    mediaPixelFormatRGB888,
    mediaPixelFormatBGR888,

    mediaPixelFormatARGB8888,
    mediaPixelFormatABGR8888,
    mediaPixelFormatARGB1555,
    mediaPixelFormatABGR1555,

    mediaPixelFormatYUYV422,
    mediaPixelFormatUYVY422,
    mediaPixelFormatYUV420P,
    mediaPixelFormatYUV422P,
    mediaPixelFormatYUV444P,
    mediaPixelFormatYUV400,      ///< 灰度图
    mediaPixelFormatYUV420M,
    mediaPixelFormatYUV422RM,
    mediaPixelFormatYUV422M,
    mediaPixelFormatYUV444M,

    mediaPixelFormatRGBA8888,
    mediaPixelFormatBGRA8888,
    mediaPixelFormatRGB565,
    mediaPixelFormatRGB888Planar,
    mediaPixelFormatBGR888Planar,

    mediaPixelFormatS16C1,      ///< 单通道有符号16bit
    mediaPixelFormatU16C1,      ///< 单通道无符号16bit
} MediaPixelFormat;

/**
 * @brief 视频流类型 RGB，IR
 */
typedef enum {
    videoStreamRGB,               ///< RGB 流
    videoStreamIR,                ///< IR 流
    videoStreamDepth,             ///< 深度图
    videoStreamDisparity,         ///< 视差图
    videoStreamConfidence,        ///< 置信度图
    videoStreamNone
} VideoStreamType;

typedef struct VideoFrameInfo_ {
    uint32_t            width;
    uint32_t            height;
    int32_t             capChan;
    VideoStreamType     stream;
    MediaPixelFormat    pixelFormat;
    uint64_t            timestamp;
    uint64_t            sequence;
    int32_t             memoryFd;       ///< 内存对应的文件句柄
    uint32_t            stride[3];      ///< 水平跨度
    uint8_t*            virAddr[3];     ///< 虚拟地址
    uint8_t*            phyAddr[3];     ///< 物理地址
    uint32_t            strideV[3];     ///< 垂直跨度
    int16_t             rotate;         ///< 被旋转的角度
    uint8_t*            priv;
    uint8_t             reserve[10];
} VideoFrameInfo;

typedef struct VideoPacketInfo_ {
#define FIRST_PKT        0x01    ///< 当前包是图像帧编码数据中的第一包
#define LAST_PKT         0x02    ///< 当前包是图像帧编码数据中的最后包
    uint32_t            width;
    uint32_t            height;
    VideoEncode         codecType;
    uint64_t            timestamp;
    uint64_t            sequence;
    uint8_t             encChan;        ///< 编码通道
    uint8_t             capChan;        ///< 采集通道
    uint8_t             nalType;        ///< 视频帧子类型(I帧-0x49 (‘I’)，P帧-0x50 (‘P’)，B帧-0x42 ('B'), 如果是JPEG，固定为‘I’，无效-0)
    uint8_t             fpsNum;         ///< 帧率分子
    uint8_t             fpsDen;         ///< 帧率分母
    uint8_t             pktInfo;        ///< 第一个包：pktInfo & FIRST_PKT != 0
                                        ///< 最后一个包：pktInfo & LAST_PKT != 0
                                        ///< 中间包：pktInfo == 0
    uint8_t             end;            ///< 是否是最后一个包，1-是（最后一个包是空包，不含数据，常用于flush操作），0-否
    uint8_t             reserve[23];
    int16_t             rotate;         ///< 被旋转的角度
} VideoPacketInfo;

typedef struct AudioFrameInfo_ {
    uint32_t            sampleRate;     ///< 采样率   8000, 16000
    uint32_t            sampleFormat;   ///< 采样位数 8, 16
    uint32_t            channelCount;   ///< 通道数   1, 2
    uint32_t            dataType;       ///< 数据类型 0, pcm, 1 wav
    uint32_t            loopTimes;      ///< 播放次数 -1 无线循环
    int32_t             memoryFd;       ///< 内存对应的文件句柄
    uint64_t            timestamp;
    uint64_t            sequence;
    uint16_t            device;         ///< 设备号
    uint16_t            channel;        ///< 通道号
    uint8_t             reserve[24];
} AudioFrameInfo;

typedef struct AudioPacketInfo_ {
    AudioEncode         codecType;
    uint64_t            timestamp;
    uint64_t            sequence;
    uint8_t             encChan;        ///< 编码通道
    uint8_t             soundMode;      ///< 声道数 1-单声道 2-双声道，其余无效
    uint8_t             soundFormat;    ///< 采样位宽 8-8位 16-16位 24-24位，其余无效
    uint32_t            sampleRate;     ///< 采样率，取值是AudioSampleRate枚举
    uint8_t             end;            ///< 是否是最后一个包，1-是（最后一个包是空包，不含数据，常用于flush操作），0-否
    uint8_t             reserve[24];
} AudioPacketInfo;

/**
 * @brief CompBuffer数据类型
 */
typedef enum {
    audioPacketBuf  = 'a',
    audioFrameBuf   = 'A',
    videoPacketBuf  = 'v',
    videoFrameBuf   = 'V',
    pictureDataBuf  = 'p',
    customedData    = 'x',
    invalidCompBuf  = 0xff
} CompBufferType;

typedef struct CompBufferInfo_ {
    float                  duration;    ///< 音视频数据播放时的持续时间，以毫秒为单位
    uint16_t               trackId;     ///< track号
    uint8_t                type;        ///< @see CompBufferType
    uint8_t                end;         ///< 是否是最后一个包，1-是（最后一个包是空包，不含数据，常用于flush操作），0-否
    uint32_t               offset;      ///< 相对于文件起始的时间偏移，单位毫秒
    uint8_t                reserve[4];
    union {
        AudioPacketInfo    audioPktInfo;    ///< 音频包信息
        AudioFrameInfo     audioFrmInfo;    ///< 音频帧信息
        VideoPacketInfo    videoPktInfo;    ///< 视频包信息
        VideoFrameInfo     videoFrmInfo;    ///< 视频帧信息
        uint8_t            xInfo[32];       ///< 自定义数据信息（暂定）
    };
} CompBufferInfo;

typedef UBase::TDelegate3<void, void*, int32_t, void*> FreeMemoryFunc;

/**
 * @brief MediaBuffer 附加元数据类型
 *
 * extraData() 已被各平台用于保存驱动资源上下文，不能复用来承载业务元数据。
 * metadata 用于跟随 MediaBuffer 生命周期携带小块业务元数据，例如 IMU。
 * metaData() 返回的 mediaBufferMetaTLV 是内部序列化 blob，仅用于跨进程传输/拷贝。
 */
typedef enum {
    mediaBufferMetaUnknown = 0,
    mediaBufferMetaTLV     = ('T' << 16) | ('L' << 8) | 'V',
    mediaBufferMetaIMU     = ('I' << 16) | ('M' << 8) | 'U',
} MediaBufferMetaType;

/**
 * @brief MediaBuffer 附加元数据只读视图
 *
 * data 指针由 MediaBuffer 持有，生命周期不超过当前 MediaBuffer 对象。
 * metaData() 返回完整序列化 blob；读取单个 metadata 使用 getMetaData()。
 */
typedef struct MediaBufferMeta_ {
    uint32_t            type;
    uint32_t            size;
    const uint8_t*      data;
} MediaBufferMeta;

typedef enum {
    imuCoordUnknown = 0,                    ///< 未知坐标系
    imuCoordSensor  = 1,                    ///< IMU 传感器原始坐标系
    imuCoordCamera  = 2,                    ///< 相机坐标系
    imuCoordBody    = 3,                    ///< 机身坐标系
} ImuCoordFrame;

typedef enum {
    imuFrameStatusOK           = 0,         ///< IMU 数据正常
    imuFrameStatusGyroEmpty    = 1u << 0,   ///< 陀螺仪数据为空
    imuFrameStatusAccEmpty     = 1u << 1,   ///< 加速度计数据为空
    imuFrameStatusGyroDelayed  = 1u << 2,   ///< 陀螺仪数据延迟
    imuFrameStatusAccDelayed   = 1u << 3,   ///< 加速度计数据延迟
    imuFrameStatusTimeInvalid  = 1u << 4,   ///< IMU 时间戳无效
    imuFrameStatusReadFailed   = 1u << 5,   ///< IMU 数据读取失败
    imuFrameStatusMetaTooLarge = 1u << 6,   ///< IMU 元数据超过允许大小
} ImuFrameStatus;

typedef struct ImuSample_ {
    uint64_t ptsUs;                         ///< IMU 样本 PTS，单位微秒
    int32_t  x;                             ///< X 轴采样值
    int32_t  y;                             ///< Y 轴采样值
    int32_t  z;                             ///< Z 轴采样值
    int32_t  temperature;                   ///< 温度采样值
} ImuSample;

typedef struct ImuFrameMetaHeader_ {
    uint16_t headerSize;                    ///< 头部大小，单位字节
    uint32_t totalSize;                     ///< 元数据总大小，包含头部和采样数据，单位字节
    uint64_t framePtsUs;                    ///< 视频帧 PTS，单位微秒
    uint64_t windowBeginUs;                 ///< IMU 采样时间窗口起始时间，单位微秒
    uint64_t windowEndUs;                   ///< IMU 采样时间窗口结束时间，单位微秒
    uint32_t gyroCount;                     ///< 陀螺仪样本数量
    uint32_t accCount;                      ///< 加速度计样本数量
    uint32_t gyroOffset;                    ///< 陀螺仪样本数组相对元数据起始位置的字节偏移
    uint32_t accOffset;                     ///< 加速度计样本数组相对元数据起始位置的字节偏移
    ImuCoordFrame coordFrame;               ///< 样本值所在坐标系，见 ImuCoordFrame
    int32_t rotation[9];                    ///< IMU 到目标坐标系的 3x3 旋转矩阵，元素取值为 -1、0、1，置位时有效
    ImuFrameStatus status;                  ///< ImuFrameStatus 位掩码，描述读取结果和数据质量
    uint8_t reserved[56];                   ///< 保留字段
} ImuFrameMetaHeader;

typedef struct ImuFrameMeta_ {
    const ImuFrameMetaHeader* header;       ///< IMU 元数据头，只读
    const ImuSample*          gyro;         ///< 陀螺仪数据数组
    const ImuSample*          acc;          ///< 加速度计数据数组
} ImuFrameMeta;

class EXPORT_API MediaBuffer {

public:

    /**
     * @brief 构造一个空的对象
     */
    MediaBuffer();
    /**
     * @brief 带参数的构造函数
     * @param[in]   data        数据指针，该数据通过func释放
     * @param[in]   size        数据大小
     * @param[in]   extraSize   扩展数据大小
     * @param[in]   func        数据释放回调
     */
    MediaBuffer(uint8_t *data, uint32_t size, uint32_t extraSize = 0, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 拷贝构造函数
     * @param[in]   other
     */
    MediaBuffer(const MediaBuffer &other);
    /**
     * @brief 赋值函数
     */
    MediaBuffer& operator=(const MediaBuffer &rhs);
    /**
     * @brief 析构函数
     */
    virtual ~MediaBuffer();

public:
    /**
     * @brief 获取数据指针
     */
    uint8_t*    data() const;
    /**
     * @brief 获取数据大小
     */
    uint32_t    size() const;
    /**
     * @brief 获取扩展数据指针
     */
    uint8_t*    extraData() const;
    /**
     * @brief 获取扩展数据大小
     */
    uint32_t    extraSize() const;
    /**
     * @brief 获取元数据只读视图
     */
    MediaBufferMeta metaData() const;
    /**
     * @brief 获取指定类型的元数据 item
     */
    bool        getMetaData(uint32_t type, MediaBufferMeta& meta) const;
    /**
     * @brief 设置/替换指定类型的元数据 item，数据会被拷贝进 MediaBuffer；size 为 0 时删除该类型
     */
    bool        setMetaData(uint32_t type, const void* data, uint32_t size);
    /**
     * @brief 删除指定类型的元数据 item
     */
    bool        removeMetaData(uint32_t type);
    /**
     * @brief 从另一个 MediaBuffer 深拷贝元数据
     */
    bool        copyMetaData(const MediaBuffer& other);
    /**
     * @brief 清空元数据
     */
    void        clearMetaData();
    /**
     * @brief 有效性判断
     */
    bool        valid() const;
    /**
     * @brief 重置对象
     */
    void        reset();
    /**
     * @brief   获取buffer对于的fd
     */
    int32_t     getFd() const;

    /**
     * @brief   设置fd，外部负责关闭
     */
    void        setFd(int32_t fd);

    /**
     * @brief 修改数据的大小
     * 
     * @param size 
     */
    bool        setSize(uint32_t size);

private:
    class Internal;
    Internal        *mInternal;
};

class EXPORT_API VideoFrame : public MediaBuffer {

public:
    /**
     * @brief 构造
     */
    VideoFrame();
    /**
     * @brief 析构
     */
    virtual ~VideoFrame();
    /**
     * @brief 构造
     * @param[in] data      数据部分
     * @param[in] size      数据长度
     * @param[in] extraSize 扩展长度
     * @param[in] func      数据析构函数
     */
    VideoFrame(uint8_t *data, uint32_t size, uint32_t extraSize = 0, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 赋值构造
     * @param[in] other
     */
    VideoFrame(const VideoFrame &other);
    /**
     * @brief = 重载
     * @param[in] rhs
     * @return
     */
    VideoFrame& operator=(const VideoFrame &rhs);

public:
    /**
     * @brief 获取video 信息
     * @return
     */
    VideoFrameInfo& getVideoFrameInfo();
    /**
     * @brief 设置视频帧信息
     * @return
     */
    const VideoFrameInfo& getFrameInfo() const;
    /**
     * @brief 获取视频帧信息
     * @param[in] frameInfo
     */
    void setFrameInfo(const VideoFrameInfo &frameInfo);

private:
    VideoFrameInfo      mFrameInfo;
};

/**
 * @brief 视频 Packet 封装
 */
class EXPORT_API VideoPacket : public MediaBuffer {

public:
    /**
     * @brief 构造
     */
    VideoPacket();
    /**
     * @brief 析构
     */
    virtual ~VideoPacket();
    /**
     * @brief 构造
     * @param[in] data      数据部分
     * @param[in] size      数据长度
     * @param[in] extraSize 扩展长度
     * @param[in] func      数据析构函数
     */
    VideoPacket(uint8_t *data, uint32_t size, uint32_t extraSize = 0, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 赋值构造
     * @param[in] other
     */
    VideoPacket(const VideoPacket &other);
    /**
     * @brief = 重载
     * @param[in] rhs
     * @return
     */
    VideoPacket& operator=(const VideoPacket &rhs);

public:
    /**
     * @brief 获取video 信息
     * @return
     */
    VideoPacketInfo& getVideoPacketInfo();
    /**
     * @brief 设置视频Packet信息
     * @return
     */
    const VideoPacketInfo& getPacketInfo() const;
    /**
     * @brief 获取视频packet信息
     * @param[in] frameInfo
     */
    void setPacketInfo(const VideoPacketInfo &frameInfo);

private:
    VideoPacketInfo     mPacketInfo;
};

/**
 * @brief 音频帧
 */
class EXPORT_API AudioFrame : public MediaBuffer {

public:
    /**
     * @brief 构造
     */
    AudioFrame();
    /**
     * @brief 析构
     */
    virtual ~AudioFrame();
    /**
     * @brief 构造
     * @param[in] data      数据部分
     * @param[in] size      数据长度
     * @param[in] extraSize 扩展长度
     * @param[in] func      数据析构函数
     */
    AudioFrame(uint8_t *data, uint32_t size, uint32_t extraSize = 0, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 赋值构造
     * @param[in] other
     */
    AudioFrame(const AudioFrame &other);
    /**
     * @brief = 重载
     * @param[in] rhs
     * @return
     */
    AudioFrame& operator=(const AudioFrame &rhs);

public:
    /**
     * @brief 获取音频帧信息
     * @return
     */
    AudioFrameInfo& getAudioFrameInfo();
    /**
     * @brief 获取音频帧信息
     * @return
     */
    const AudioFrameInfo& getFrameInfo() const;
    /**
     * @brief 设置音频帧信息
     * @param[in] frameInfo
     */
    void setFrameInfo(const AudioFrameInfo &frameInfo);
private:
    AudioFrameInfo      mFrameInfo;
};

/**
 * @brief 音频packet封装
 */
class EXPORT_API AudioPacket : public MediaBuffer {

public:
    /**
     * @brief 构造
     */
    AudioPacket();
    /**
     * @brief 析构
     */
    virtual ~AudioPacket();
    /**
     * @brief 构造
     * @param[in] data      数据部分
     * @param[in] size      数据长度
     * @param[in] extraSize 扩展长度
     * @param[in] func      数据析构函数
     */
    AudioPacket(uint8_t *data, uint32_t size, uint32_t extraSize = 0, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 赋值构造
     * @param[in] other
     */
    AudioPacket(const AudioPacket &other);
    /**
     * @brief = 重载
     * @param[in] rhs
     * @return
     */
    AudioPacket& operator=(const AudioPacket &rhs);

public:
    /**
     * @brief 获取音频帧信息
     * @return
     */
    AudioPacketInfo& getAudioPacketInfo();
    /**
     * @brief 获取音频帧信息
     * @return
     */
    const AudioPacketInfo& getPacketInfo() const;
    /**
     * @brief 设置音频帧信息
     * @param[in] frameInfo
     */
    void setPacketInfo(const AudioPacketInfo &frameInfo);

private:
    AudioPacketInfo     mPacketInfo;
};

class EXPORT_API CompBuffer : public MediaBuffer {

public:
    /**
     * @brief 构造
     */
    CompBuffer();
    /**
     * @brief 析构
     */
    virtual ~CompBuffer();
    /**
     * @brief 构造
     * @param[in] data      数据部分
     * @param[in] size      数据长度
     * @param[in] extraSize 扩展长度
     * @param[in] func      数据析构函数
     */
    CompBuffer(uint8_t *data, uint32_t size, uint32_t extraSize = 0, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 赋值构造
     * @param[in] other
     */
    CompBuffer(const CompBuffer &other);
    /**
     * @brief = 重载
     * @param[in] rhs
     * @return
     */
    CompBuffer& operator=(const CompBuffer &rhs);

public:
    /**
     * @brief 获取缓存具体信息
     * @return
     */
    const CompBufferInfo& getCompBufferInfo() const;
    /**
     * @brief 设置缓存具体信息
     * @param[in] info
     */
    void setCompBufferInfo(const CompBufferInfo& info);

private:
    CompBufferInfo     mCompBufferInfo;
};

}
}

#endif //__UFACE_MEDIA_MEDIA_FRAME_H__
