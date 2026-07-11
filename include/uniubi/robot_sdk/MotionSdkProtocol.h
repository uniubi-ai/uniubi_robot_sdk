/**
 * ========================================================
 *  @file MotionSdkProtocol.h
 *  @brief  Motion SDK 公开协议数据类型（开源面）
 *  @author shangyang
 *  @date 2026-04-15
 *  @version 1.0
 *  @details
 *      - SDK 用户面 POD 协议结构（控制输入、观测输出）
 *      - 仅依赖 <cstdint> + <type_traits>，零第三方
 *      - 网络字节序：大端；wire 紧凑布局：#pragma pack(push, 1)
 * ========================================================
 */

#ifndef ROBOTSERVICE_MOTION_SDK_PROTOCOL_H
#define ROBOTSERVICE_MOTION_SDK_PROTOCOL_H

#include <cstdint>
#include <type_traits>

namespace uniubi {
namespace RobotSdk {

static constexpr uint16_t kLowLevelMaxMotorNum = 16u;
static constexpr uint16_t kMotionActionLength  = 32u;
/**
 * @brief 遥控器按键
 */
typedef enum {
    buttonBack,
    buttonStart,
    buttonLB,
    buttonRB,
    buttonF1,
    buttonF2,
    buttonA,
    buttonB,
    buttonX,
    buttonY,
    buttonUp,
    buttonDown,
    buttonLeft,
    buttonRight,
    buttonLS,
    buttonRS,
    BUTTON_MAX
} ButtonDefine;

/**
 * @brief 摇杆数据
 */
typedef enum {
    axesLX,
    axesLY,
    axesRX,
    axesRY,
    axesLT,
    axesRT,
    AXES_MAX
} AxesDefine;

/**
 * @brief gps 信号等级
 */
typedef enum {
    gpsGre38db = 0,        /**信号强*/
    gpsGre30db = 1,        /**信号中*/
    gpsLes30db = 2         /**信号弱*/
} GPSSignalLevel;

/**坐标类型*/
typedef enum {
    gcj02,          /**国测局坐标-谷歌中国地图/腾讯地图/高德地图*/
    wgs84,          /**GPS 坐标，谷歌地球使用*/
    bd09,           /**百度坐标系，百度地图使用*/
    mapBar,         /**图吧坐标，图吧地图使用*/
} GEOGCoordMode;

/**
 * @brief 运控模式（大脑/小脑）
 */
typedef enum {
    cerebellumMode,                 /**小脑执行运控*/
    brainControlMode,               /**大脑执行运控*/
    motionModeNotReady = 255,       /**运控模式未准备好*/
} MotionControlMode;

typedef enum {
    uwbPairNone,                    /**未配对*/
    uwbInPairing,                   /**配对中*/
    uwbPairSuccess,                 /**配对成功*/
    uwbPairFailed,                  /**配对失败*/
} UWBPairState;

/**
 * @brief IMU 读数错误码（Vector3f / Quaternionf 的 error 字段；
 *        accel / gyro / mag / euler / quaternion 各路独立标识）
 *        0/1 由 IMU 上报，64+ 由 SDK / 服务端检测后填入
 */
typedef enum {
    imuNormal           = 0,        ///< 数据有效
    imuInvalid          = 1,        ///< IMU 数据无效（该量当前不可信）
    imuControlOffline   = 1 << 6,   ///< IMU 控制板离线
    imuControlNotReady,             ///< 控制板未就绪
    imuControlUpgrade,              ///< 控制板升级中
    imuControlNotParams,            ///< IMU 模组参数未就绪
    imuNotReady,                    ///< IMU 加热 / 未就绪
} IMUDeviceErrno;

/**
 * @brief 电机故障码（MotorObserved.error）
 *        0~6/59/60/63 由电机上报，64+ 由 SDK / 服务端检测后填入
 */
typedef enum {
    motorNormal          = 0,       ///< 正常
    motorPreDriver       = 1,       ///< 预驱故障
    motorEcodeError      = 2,       ///< 编码器故障
    motorOverSpeed       = 3,       ///< 过速
    motorOverTempe       = 4,       ///< 过温
    motorOverCurrent     = 5,       ///< 过流
    motorOverVoltage     = 6,       ///< 过压
    motorPGAbnormality   = 0x3B,    ///< PG 异常
    motorHWUndervoltage  = 0x3C,    ///< 硬件欠压
    motorCommError      = 0x3F,    ///< 通信错误
    motorControlOffline  = 1 << 6,  ///< 控制板离线
    controlMotorNotEnable,          ///< 未使能
    motorControlNotReady,           ///< 控制未就绪
    motorControlUpgrade,            ///< 升级中
    motorNoCalibrated,              ///< 未标定
    motorURDFNotMapped,             ///< 标定丢失
} MotorDeviceErrno;

typedef enum {
    motorCereControl    = 0,
    motorBrainControl   = 1,
    motorUnknownCtrl    = 255,
} MotorMasterMode;

#pragma pack(push, 1)

struct MediaLayout {
    uint32_t       micNum = 0;             /** 语音采集mic数量*/
    uint32_t       cameraNum = 0;          /** 摄像头数量*/
    uint32_t       videoEncoderNum = 0;    /** 视频编码通道数*/
};

static_assert(sizeof(MediaLayout) == 12, "MediaLayout size invalid");

struct Vector3f {
    int8_t          error;   ///< 0=正常,非0=该读数无效
    float           x;
    float           y;
    float           z;
};
static_assert(sizeof(Vector3f) == 13, "Vector3f size invalid");

struct Quaternionf {
    int8_t   error;   ///< 0=正常,非0=无效
    float    w;       ///< 单位四元数,无量纲([-1,1])
    float    x;
    float    y;
    float    z;
};
static_assert(sizeof(Quaternionf) == 17, "Quaternionf size invalid");

struct IMUObserved {
    float                 temp;        ///< 温度,℃
    Vector3f              accel;       ///< 加速度,m/s²(x/y/z)
    Vector3f              gyro;        ///< 角速度,rad/s(x/y/z)
    Vector3f              mag;         ///< 磁场,μT(x/y/z)
    Vector3f              euler;       ///< 欧拉角,rad(x=roll [-π,π]/y=pitch [-π/2,π/2]/z=yaw [-π,π])
    Quaternionf           quaternion;  ///< 姿态四元数,无量纲
};
static_assert(sizeof(IMUObserved) == 73, "IMUObserved size invalid");

struct PowerObserved {
    float           power  = 0.0;             /**< 当前电池电量,%*/
    float           health = 0.0;             /**< 健康度,%*/
    float           temper = 0.0;             /**< 电池温度,℃(有符号)*/
    float           chargeCurrent = 0.0;      /**< 实时电流,A*/
    float           chargeVoltage = 0.0;      /**< 当前总电压,V*/
};
static_assert(sizeof(PowerObserved) == 20, "PowerObserved size invalid");

struct UWBRawObserved {
    uint8_t         valid = 0;              /**< 数据是否有效*/
    uint8_t         pairState = 0;          /**< 是否配对,见 UWBPairState*/
    int16_t         rssi = 0;               /**< 信号强度 单位 dbm*/
    uint16_t        pitch = 0;              /**< 俯仰角 单位 deg,[0, 360)，用于将空间距离投影到水平距离*/
    uint16_t        azimuth = 0;            /**< 方位角 单位 deg,[0, 360) 正前方 0 度、逆时针递增*/
    uint32_t        distance = 0;           /**< 距离 单位 cm*/
};
static_assert(sizeof(UWBRawObserved) == 12, "UWBRawObserved size invalid");

struct MotorHeader {
    uint16_t       limbNo;
    uint16_t       jointNo;
};
static_assert(sizeof(MotorHeader) == 4, "MotorHeader size invalid");

/// 单个电机的硬件描述（getMotorLayout 返回）
struct MotorInfo {
    uint16_t       limbNo;             ///< 肢体编号
    uint16_t       jointNo;            ///< 该肢体内关节编号
    char           name[28];           ///< 关节人类可读名（"<limb>.<joint>"，如 "leftFront.hip"）
};
static_assert(sizeof(MotorInfo) == 32, "MotorInfo size invalid");

/// 整机电机布局（getMotorLayout 返回，连接后查询，硬件配置不变）
struct MotorLayout {
    uint32_t       motorNum;                          ///< 实际启用电机数
    MotorInfo      motors[kLowLevelMaxMotorNum];      ///< 每电机的描述
};
static_assert(sizeof(MotorLayout) == 4 + 32 * kLowLevelMaxMotorNum, "MotorLayout size invalid");

struct MotorObserved {
    uint8_t         enable;         /**< 1=已使能*/
    uint8_t         online;         /**< 1=在线*/
    uint8_t         error;          /**< 0=正常,非0=故障码*/
    float           position;       /**< 关节角,rad*/
    float           velocity;       /**< 关节角速度,rad/s*/
    float           torque;         /**< 力矩,N·m*/
    float           temp;           /**< 电机温度,℃*/
    float           voltage;        /**< 电机电压,V*/
    float           lossRate;       /**< 电机丢包率,%*/
    float           maxTorque;      /**< 电机最大扭矩,N·m*/
    MotorHeader     header;
};
static_assert(sizeof(MotorObserved) == 35, "MotorObserved size invalid");

struct TRCStickFrame {
    uint32_t              valid;              /**< 是否有效*/
    uint8_t               buttons[BUTTON_MAX];/**< 按键*/
    float                 axes[AXES_MAX];     /**< 摇杆*/
    uint64_t              controlId;          /**< 控制id*/
};
static_assert(sizeof(TRCStickFrame) == 52, "TRCStickFrame size invalid");

/**地理坐标点*/
struct GEOGPoint {
    float                lat = 0.0f;               /**纬度,deg*/
    float                lng = 0.0f;               /**经度,deg*/
};
static_assert(sizeof(GEOGPoint) == 8, "GEOGPoint size invalid");

struct GPSFrame {
    uint32_t         valid = 0;              /**< 1=有效,0=异常*/
    float            speed = 0;              /**< GPS 测速,km/h*/
    int32_t          level = 0;              /**< 信号等级,见 GPSSignalLevel*/
    int32_t          rssi  = 0;              /**< 信号强度原始值 单位 dbm*/
    GEOGPoint        point;                  /**< 坐标信息*/
};
static_assert(sizeof(GPSFrame) == 24, "GPSFrame size invalid");

struct SensorObserved {
    GPSFrame         gps;
    UWBRawObserved   uwb;
};

static_assert(sizeof(SensorObserved) == 36, "SensorObserved size invalid");

/**
 * @brief 低级运控操作指令
 */
struct LowLevelMotionCmd {
    int32_t    action = -1;
    char       acName[kMotionActionLength];
    float      velocity = 0.0f;
    float      velocityX = 0.0f;
    float      velocityY = 0.0f;
};

struct MotorCtrl {
    float           position;   /**< 目标关节角,rad*/
    float           velocity;   /**< 目标关节角速度,rad/s*/
    float           kpGain;     /**< 位置刚度,N·m/rad*/
    float           kdGain;     /**< 速度阻尼,N·m·s/rad*/
    float           torque;     /**< 前馈力矩,N·m*/
    MotorHeader     header;
};
static_assert(sizeof(MotorCtrl) == 24, "MotorCtrl size invalid");

struct MotorCtrlAction {
    uint32_t           motorNum;
    MotorCtrl          motors[kLowLevelMaxMotorNum];
};
static_assert(sizeof(MotorCtrlAction) == 388, "MotorCtrlAction size invalid");

struct LowLevelMotionObserved {
    uint8_t                systemSta;
    uint32_t               motorNum;
    IMUObserved            imu;
    TRCStickFrame          trc;
    PowerObserved          power;
    MotorObserved          motors[kLowLevelMaxMotorNum];
};
static_assert(sizeof(LowLevelMotionObserved) == 710, "LowLevelMotionObserved size invalid");

#pragma pack(pop)

static_assert(std::is_standard_layout<LowLevelMotionObserved>::value, "LowLevelMotionObserved must be standard layout");
static_assert(std::is_trivially_copyable<LowLevelMotionObserved>::value, "LowLevelMotionObserved must be trivially copyable");

static_assert(std::is_standard_layout<MotorCtrlAction>::value, "MotorCtrlAction must be standard layout");
static_assert(std::is_trivially_copyable<MotorCtrlAction>::value, "MotorCtrlAction must be trivially copyable");
static_assert(std::is_standard_layout<SensorObserved>::value, "MotionSensorObserved must be standard layout");
static_assert(std::is_trivially_copyable<SensorObserved>::value, "MotionSensorObserved must be trivially copyable");

} // namespace RobotSdk
} // namespace uniubi

#endif // ROBOTSERVICE_MOTION_SDK_PROTOCOL_H
