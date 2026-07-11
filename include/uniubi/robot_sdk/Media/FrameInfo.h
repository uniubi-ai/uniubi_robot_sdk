/************************************************
 * Copyright(c) 2020 uni-ubi
 * 
 * Project:    Stream
 * FileName:   FrameInfo.h
 * Author:     shangyang
 * Email:      shangyang@uni-ubi.com
 * Version:    V1.0.0
 * Date:       2022-10-10 15:39
 * Description: 
 * Others:
 *************************************************/

#ifndef _DEPEND_INCLUDE_STREAM_FRAMEINFO_H_
#define _DEPEND_INCLUDE_STREAM_FRAMEINFO_H_

#include "uniubi/robot_sdk/Media/Define.h"

namespace Uface {
namespace Stream {

/**
 * @brief 码流类型
 */
typedef enum {
    main = 0,               ///< 主码流
    extra1,                 ///< 辅码流1
    extra2,                 ///< 辅码流2
    extra3,                 ///< 辅码流3
    snapshot,               ///< 抓图
    talkback,               ///< 对讲流
    tapeIn,                 ///< 录音输入
    streamNumber            ///< 种类数
} StreamType;

/**
 * @brief 同源数据trackid
 */
typedef enum {
    videoTrack,             ///< 视频数据
    audioTrack              ///< 音频数据
} StreamTrack;

/**
 * @brief 帧类型
 */
typedef enum {
    audioFrame      = 0x41,         /**'A' 音频帧*/
    videoFrame      = 0x56,         /**'V' 视频帧*/
    auxFrame        = 0x58,         /**'X' 辅助帧*/
    imageFrame      = 0x4A,         /**'J' 图片帧*/
    videoBFrame     = 0x42,         /**'B' 视频-B帧*/
    videoIFrame     = 0x49,         /**'I' 视频-I帧*/
    videoPFrame     = 0x50,         /**'P' 视频-P帧*/
    waterFrame      = 0x57,         /**'W' 辅助帧-水印帧*/
    auxMetaFrame    = 0x4D          /**'M' 辅助帧-元数据帧*/
} FrameType;

/**
 * @brief 流变化标签
 */
typedef enum {
    streamNoChange          = 0x00,
    streamContentChange     = 0x01,
    streamEncodeChange      = 0x02,
    streamOtherChange       = 0x03
} StreamChange;

#pragma pack (1)
/**
 * @brief 视频帧信息
 */
typedef struct {
    uint8_t         stream;            /**流类型 @see StreamType*/
    uint8_t         encode;            /**编码类型 @see Media::VideoEncode*/
    uint8_t         type;              /**'I'/'P'/'B'/'J' @see FrameType*/
    uint8_t         fpsNum;            /**帧率分子*/
    uint8_t         fpsDen;            /**帧率分母*/
    uint16_t        width;             /**视频宽*/
    uint16_t        height;            /**视频高*/
    uint8_t         res[2];            /**保留*/
} VFrameInfo;

typedef struct {
    uint8_t         encode;            /**编码类型 @see Media::ImageEncode*/
    uint16_t        width;             /**视频宽*/
    uint16_t        height;            /**视频高*/
    uint8_t         res[2];            /**保留*/
} ImageInfo;

/**
 * @brief 音频帧信息
 */
typedef struct {
    uint8_t         encode;             /**编码类型 @see Media::AudioEncode*/
    uint8_t         sampleBit;          /**编码位数*/
    uint8_t         rate;               /**采样率*/
    uint8_t         channel;            /**声道*/
} AFrameInfo;

/**
 * @brief 扩展帧信息
 */
typedef struct {
    uint8_t         stream;             /**流类型 @see StreamType*/
    uint8_t         type;               /**'W'/'M'*/
} EFrameInfo;

/**
 * @brief 帧信息定义
 */
typedef struct {

    int8_t          type;               /**帧类型-'A'/'V'/'J'/'X'*/
    int8_t          channel;            /**帧编码的通道*/
    uint8_t         encrypt;            /**帧加密方式-预留*/
    uint8_t         change;             /**流变化状态,@see StreamChange*/
    uint16_t        utcms;              /**帧utc 时间s对应的ms*/
    uint32_t        seq;                /**帧序号*/
    uint32_t        utc;                /**帧utc 时间-单位s*/
    uint32_t        length;             /**帧长度*/
    uint64_t        pts;                /**帧显示时间戳- 单位ms*/
    uint64_t        ptsNet;             /**网络模块获取到帧的时间戳-单位ms*/
    union {
        VFrameInfo  video;              /**视频信息*/
        AFrameInfo  audio;              /**音频信息*/
        ImageInfo   image;              /**图片信息*/
        EFrameInfo  extra;              /**辅助帧信息*/
    } detail;

    void*           priPtr;             /**指针*/
    uint32_t        reserved[8];        /**保留位*/

} FrameInfo;

#pragma pack ()

}
}
#endif //_DEPEND_INCLUDE_STREAM_FRAMEINFO_H_
