/**
 * ========================================================
 *  @file MotionLowLevelClient.h
 *  @brief 
 *  @author shangyang
 *  @date 2026-05-06 11:06
 *  @version 1.0
 *  @details 
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 *  @license 
 * ========================================================
 */

#ifndef ROBOTSERVICE_MOTIONLOWLEVELCLIENT_H
#define ROBOTSERVICE_MOTIONLOWLEVELCLIENT_H

#include <memory>
#include <cstdint>
#include <functional>
#include "uniubi/robot_sdk/MediaBusClient.h"
#include "uniubi/robot_sdk/MotionSdkProtocol.h"

namespace uniubi {
namespace RobotSdk {

class IMotionLowLevelClient {

public:

    typedef enum {
        kDisconnected = 0,
        kConnecting,        ///< 首次连接尝试中（尚未连上）
        kConnected,         ///< 控制面 + SHM 通道就绪，但 prepare 未开 → sendControl 无效
        kPrepared,          ///< prepare 已开，sendControl 才会被服务端采纳
        kConnectionLost,    ///< 曾经 kConnected/kPrepared，因 session 失效 / server 异常掉线，worker 正在重连
    } LowLevelState;

    typedef enum {
        kNone = 0,
        kRpcConnectFailed,      ///< SDK 未初始化（MotionSdkRpcClient/BusService 未就绪）
        kRpcAcquireRejected,    ///< acquireMode 被拒（无控制权 / server 拒绝/server 端无响应）
        kPrepareRejected,       ///< setMotionPrepare(true) 失败（运控开关）
        kSessionExpired,
        kSessionReleased,
        kServerStop,
        kRpcCallFailed,         ///< RPC 调用失败（含超时 / 通道断 / 编解码错 / id 不匹配等）
        kNotConnected,
        kNotPrepared,           ///< 当前未 prepare，无法下发控制
        kInvalidArgument,       ///< 入参非法（如 motorNum == 0 或 > kLowLevelMaxMotorNum）
        kMasterSwitchFailed,    ///< 切换运控 master 失败（sdk 内部会自动切换 被拒/无响应）
    } LowLevelError;

    /// 连接状态回调；state 为最新状态（kConnecting/kConnected/kPrepared/kConnectionLost/kDisconnected），
    /// error 携带本次状态变化的具体原因（kNone 表示无错误）。
    /// 任何会话事件（expired/released/serverStop）会被视为"逻辑断连"，统一通过本回调上报，
    /// state 转入 kConnectionLost；worker 后续会自动尝试重连，成功后回 kConnected
    using ConnectCallback = std::function<void(LowLevelState state, LowLevelError error)>;
public:
    /**
     * @brief 创建低级控制客户端（仅本机板内部署，直连本地 MotionServer）
     * @return 失败返回空指针
     */
    static std::shared_ptr<IMotionLowLevelClient> create();
public:
    /**
     * @brief 断开连接
     */
    virtual void disconnect() = 0;
    /**
     * @brief 当前连接状态
     */
    virtual int32_t getState() const = 0;
    /**
     * @brief 获取最后一次失败原因
     */
    virtual int32_t getLastError() const = 0;
    /**
     * @brief 切换运控使能状态（异步）
     * @param enable true=请求开启 motion（state 推进至 kPrepared），false=关闭（回到 kConnected）
     * @return true=请求已受理；false=请求被拒（如当前 kDisconnected）。
     *         真正生效与否由 worker 异步推进，调用方通过 getState() 轮询或 ConnectCallback 感知。
     *         RPC 暂时失败 worker 会自动重试，调用方一次 setMotionEnable 即可。
     */
    virtual bool setMotionEnable(bool enable) = 0;
    /**
     * @brief 紧急急停
     * @param timeout
     * @return
     */
    virtual bool emergencyStop(uint32_t timeout = 5000) = 0;
    /**
     * @brief 注册连接结果回调（在 worker 线程中调用）
     */
    virtual void setConnectCallback(ConnectCallback cb) = 0;
    /**
     * @brief 创建音视频客户端通道
     * @return
     */
    virtual IMediaBusClient::Ptr createMediaBusClient() = 0;
    /**
     * @brief 运控模式恢复默认
     * @param timeout ms
     * @return
     */
    virtual bool restoreMotionControlMode(uint32_t timeout = 5000) = 0;
    /**
     * @brief 连接到 MotionServer（非阻塞，内部 worker 线程自动完成全流程，幂等）
     *        连接结果通过 getState()/getLastError()/ConnectCallback 感知
     *        需要先调用 IMotionSdkService::instance()->initialService()
     * @param observedHz  期望观测频率（Hz）
     * @param leaseMs     控制权租约时长（ms）；0 = 用 server 默认。
     *                    motionServer 会按自身策略 clamp/校验后下发真实值；
     *                    SDK 拿到响应后按真实值算续约间隔（lease/3）
     * @return true 启动连接流程成功（不代表已连接，仅表示流程启动成功）；
     *              已 kConnecting/kConnected/kPrepared 时直接返回 true
     *
     * @note 超时与重试策略：
     *   - SDK 内部对 connect 流程 **不设整体超时**，worker 会按 reconnectBackoff（配置项）
     *     无限循环重试 takeWorkModeSession / openShmChannel / setMotionPrepare，
     *     直到成功，或被 disconnect() 显式打断
     *   - 每一次中间环节失败（RPC 拒绝 / sessionId 字段无效 / SHM 打开失败 /
     *     setMotionPrepare 被拒）都会触发
     *     ConnectCallback(state, error)，调用方可据此感知进度
     *   - 调用方如需 "N 秒 / N 次失败放弃" 语义，自行在 ConnectCallback 中累计
     *     失败次数或比对墙上时间，达到阈值后调用 disconnect() 终止 worker 即可
     *     （SDK 不主动放弃，给调用方完整决策权）
     */
    virtual bool connect(uint32_t observedHz = 500,uint32_t leaseMs = 0) = 0;
    /**
     * @brief 获取电机硬件布局（kConnected 后即可调用，硬件配置启动后不变；SDK 内部缓存）
     * @param layout 输出：电机数 + 每电机的 (limbNo, jointNo, name)
     * @param timeout RPC 超时（ms），首次调用走 RPC；之后缓存命中无延迟
     * @return true 拿到布局；false 未连接 / RPC 失败 / 配置无效
     *
     * 典型用法：
     *     MotorLayout layout = {};
     *     client->getMotorLayout(layout);
     *     for (uint32_t i = 0; i < layout.motorNum; ++i) {
     *         action.motors[i].header.limbNo  = layout.motors[i].limbNo;
     *         action.motors[i].header.jointNo = layout.motors[i].jointNo;
     *         // layout.motors[i].name = "leftFrontLeg.hip" 等可读名
     *     }
     */
    virtual bool getMotorLayout(MotorLayout& layout, uint32_t timeout = 5000) = 0;
    /**
     * @brief 获取传感器数据(GPS + UWB)
     * @details 与运控 prepare 无关,kConnected / kPrepared 任一状态均可读(传感器常驻采集);
     *          无传感器硬件的设备无数据写入,会等待至超时返回 false
     * @param sensor 输出:GPS + UWB 原始观测
     * @param timeout 等待数据的超时(us)
     * @return true 拿到数据;false 未连接 / 超时 / 无数据
     */
    virtual bool getSensorObservation(SensorObserved* sensor,uint32_t timeout) = 0;
    /**
     * @brief 获取最新观测量（拉模式）
     * @param obs 输出观测量
     * @return true 有数据
     */
    virtual bool getLatestObservation(LowLevelMotionObserved* obs, uint32_t timeout) = 0;
    /**
     * @brief 发送控制帧（仅 kPrepared 状态下生效，否则返回 false 并置 lastError）
     * @param action 电机控制数据
     * @param cmd 可以为空；动作相关控制帧建议传入并填写 action/acName;
     */
    virtual bool sendControl(const MotorCtrlAction& action,const LowLevelMotionCmd* cmd = nullptr) = 0;
};
}
}
#endif //ROBOTSERVICE_MOTIONLOWLEVELCLIENT_H
