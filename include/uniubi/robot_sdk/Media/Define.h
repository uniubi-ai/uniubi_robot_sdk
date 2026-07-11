/************************************************
 * Copyright(c) 2020 uni-ubi
 *
 * Project:    Media
 * FileName:   MediaService.h
 * Author:     tianfu
 * Email:      tianfu@uni-ubi.com
 * Version:    V1.0.0
 * Date:       2020-09-09 10:11
 * Description:
 * Others:
 *************************************************/

#ifndef __UFACE_MEDIA_DEFINE_H__
#define __UFACE_MEDIA_DEFINE_H__

#include "uniubi/robot_sdk/UBase/Define.h"

namespace Uface {
namespace Media {

/**
* @brief video encode type
*/
typedef enum{
    vInvalidCodec     = 0,
    videoH264         = 1,
    videoH265         = 2,
    videoMpeg4        = 3,
    videoMJpeg        = 4,
    videoJpeg         = 5,
    videoCodecButt
} VideoEncode;

/**
 * @brief 图像编码定义
 */
typedef enum {
    mInvalidCodec     = 0,
    imageMJPEG        = 1,
    imageJPED         = 2,
    imageCodecButt
} ImageEncode;
/**
* @brief 视频编码Profile
*/
typedef enum {
    videoProfileNull,
    videoProfileH264Base,
    videoProfileH264Main,
    videoProfileH264High,
    videoProfileH264Svc,
    videoProfileH265Main,
    videoProfileH265Main10,
    videoProfileJpeg,
    videoProfileButt
} VideoEncodeProfile;

/**
* @brief 编码控制方式
*/
typedef enum {
    videoRCNone,
    videoRCCBR,             /**< 定码率（在码率统计时间内保证编码码率平稳）*/
    videoRCVBR,             /**< 变码率（允许在码率统计时间内编码码率波动，从而保证编码图像质量平稳）*/
    videoRCCVBR,            /**< 可认为是码率受限的VBR*/
    videoRCAVBR,            /**< 自适应变码率（根据场景运动情况动态调整）*/
    videoRCFIXQP,           /**< 固定图像质量码率控制方式*/
    videoRCQVBR,            /**< 基于PSNR的变码率控制方式*/
    videoRCButt,
} VideoEncodeRc;

/**
* @brief 分辨率定义
*/
typedef enum {
    videoResNone,
    videoRes352x288          = 4,
    videoRes640x480          = 10,
    videoRes800x600          = 12,
    videoRes704x576          = 13,
    videoRes960x540          = 15,
    videoRes1280x720         = 16,
    videoRes1280x960         = 17,
    videoRes1280x1024        = 19,
    videoRes1920x1080        = 28,
    videoRes2048x1080        = 29,
    videoRes2560x1440        = 30,
    videoRes3840x2160        = 34,
    videoRes4096x2160        = 35,
    videoRes7680x4320        = 40,
    videoResButt
} VideoResolution;

/**
* @brief audio encode type
*/
typedef enum{
    audioAAC      = 0,
    audioG711a    = 1,
    audioG711u    = 2,
    audioG726     = 3,
    audioPCM      = 4,
    audioMP3      = 5,
    audioOpus     = 6,
    audioButt
} AudioEncode;

/**
* @brief 音频采样点数据位宽格式枚举值
*/
typedef enum {
    audioBitWidthNone    = 0,
    audioBitWidth8Bit    = 8,       /**< 8bit width */
    audioBitWidth16Bit   = 16,      /**< 16bit width */
    audioBitWidth24Bit   = 24,      /**< 24bit width */
    audioBitWidthButt
} AudioBitWidths;

/**
* @brief 音频声道数枚举值
*/
typedef enum{
    audioSoundModeNone     = 0,
    audioSoundModeMono     = 1, /**< 单声道 */
    audioSoundModeStereo   = 2, /**< 双声道 */
    audioSoundModeButt
} AudioSoundMode;

/**
* @brief audio sample rate
*/
typedef enum{
    audioSampleRateNone   = 0,
    audioSampleRate8000  ,
    audioSampleRate11025 ,
    audioSampleRate16000 ,
    audioSampleRate22050 ,
    audioSampleRate32000 ,
    audioSampleRate44100 ,
    audioSampleRate48000 ,
    audioSampleRate64000 ,
    audioSampleRate96000 ,
    audioSampleRateButt
} AudioSampleRate;

}
}

#endif //__UFACE_MEDIA_DEFINE_H__
