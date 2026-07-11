/**
 * ========================================================
 *  @file MotionHighLevelClient.h
 *  @brief  高级控制模式客户端 SDK 公开接口
 *  @author shangyang
 *  @date 2026-05-07
 *  @version 1.0
 *  @details
 *      高级模式：SDK -> RobotServer RPC -> MotionServer RPC
 *      控制权由 startControl 显式获取、SDK 内部周期续约维持，
 *      直到 releaseControl / disconnect 主动释放，或服务端 session 超时被动失效。
 *      只有控制面，没有独立数据面（状态以 query 接口返回）。
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 * ========================================================
 */

#ifndef ROBOTSERVICE_MOTIONHIGHLEVELCLIENT_H
#define ROBOTSERVICE_MOTIONHIGHLEVELCLIENT_H

#include <string>
#include <memory>
#include <functional>
#include "uniubi/robot_sdk/MediaBusClient.h"
#include "uniubi/robot_sdk/MotionSdkProtocol.h"

namespace uniubi {
namespace RobotSdk {

class IMotionHighLevelClient {

public:

    typedef enum {
        kDisconnected = 0,
        kConnected,         ///< SDK 就绪，但未持有控制权
        kControlled,        ///< 已通过 startControl 获取控制权，可下发动作
    } HighLevelState;

    typedef enum {
        kNone = 0,
        kRpcConnectFailed,      ///< SDK 未初始化
        kRpcAcquireRejected,    ///< startControl 被拒（acquireMode 失败 / 控制权被他人占用）
        kRpcCallFailed,         ///< RPC 调用失败（含超时 / 通道断 / 编解码错 / id 不匹配等）
        kSessionExpired,        ///< lease 到期被动失效（host proxy reaper 收回）
        kSessionRevoked,        ///< 被另一方接管 / sessionId 不再匹配（强制踢出）
        kNotConnected,          ///< 未 connect 时调用需要连接的接口
        kNotControlled,         ///< 未持有控制权时调用动作类接口
        kDataNotUpdate,         ///< 观测量数据面未就绪
        kActionRejected,
        kInvalidParam,          ///< 入参非法（如动作参数 JSON 解析失败）
    } HighLevelError;
    /**
     * GPS 数据回调；observed 上报开启后随帧推送
     */
    using GPSCallback = std::function<void(const GPSFrame& gps)>;
    /**
     * 运控观测量回调；observed 上报开启后随帧推送（含 power）
     */
    using MotionObservedCallback = std::function<void(const LowLevelMotionObserved& obs)>;
    /// 控制权状态变化回调；在 worker 线程中调用，state = 切换后的 HighLevelState
    /// 典型场景：
    /// - 成功获取（startControl）：state=kControlled, error=kNone
    /// - 自己 release 完成：       state=kConnected,  error=kNone
    /// - acquire 超时被拒：        state=kConnected,  error=kRpcAcquireRejected
    /// - lease 到期：              state=kConnected,  error=kSessionExpired
    /// - 被另一方接管：            state=kConnected,  error=kSessionRevoked
    using ConnectCallback = std::function<void(HighLevelState state, HighLevelError error)>;
    /// 事件回调；在 EventBus 派发线程中调用，payloadJson 为 UTF-8 JSON 字符串
    using EventCallback = std::function<void(const std::string& topic, const std::string& payloadJson)>;
public:
    /**
     * @brief 创建高级控制客户端
     * @note 本地设备端部署
     * @param asMaster 是否作为控制运控
     * @return
     */
    static std::shared_ptr<IMotionHighLevelClient> create(bool asMaster = false);
    /**
     * @brief 创建高级控制客户端
     * @note 网络模式
     * @return
     */
    static std::shared_ptr<IMotionHighLevelClient> create(std::string deviceId);
public:
    /**
     * @brief 退出高级模式（如持有控制权会先 releaseControl）
     */
    virtual void disconnect() = 0;
    /**
     * @brief 异步请求释放控制权（立即返回，worker 推进 RPC releaseMode → state 切回 kConnected）
     * @return true 请求已受理；释放完成时 ConnectCallback(kConnected, kNone) 通知
     */
    virtual bool releaseControl() = 0;
    /**
     * @brief 当前状态（HighLevelState）
     */
    virtual int32_t getState() const = 0;
    /**
     * @brief 获取最后一次失败原因
     */
    virtual int32_t getLastError() const = 0;
    /**
     * @brief 标记进入高级模式（幂等：已 connect / kControlled 时直接返回 true）
     * @param leaseMs SDK 期望的控制权租约时长（ms）；<=0 时 SDK 内部默认填 60000（60s）。
     *                Host proxy 会按 [5000, 300000] 范围 clamp 后下发真实值，
     *                可通过后续 startControl 成功事件感知最终生效的 lease。
     */
    virtual bool connect(int32_t leaseMs = 0) = 0;
    /**
     * @brief 注册 GPS 数据回调（observed 上报开启后随帧推送）
     */
    virtual void setGPSCallback(GPSCallback cb) = 0;
    /**
     * @brief 注册服务端事件回调（EventBus 派发线程触发）
     *        必须在 connect 之前调用，connect 时会用此回调订阅 RobotServer 的事件 topic：
     *          - statistics/play_list      —— 播放状态变化（1.2.2）
     *          - statistics/device_status  —— 系统状态变化（2.2）
     *        控制权变化（robot/attributes）不走此回调 —— SDK 内部消费切 HighLevelState +
     *        ConnectCallback(kConnected, kSessionRevoked) 反映。
     */
    virtual void setEventCallback(EventCallback cb) = 0;
    /**
     * @brief 注册控制权状态变化回调（worker 线程触发）
     */
    virtual void setConnectCallback(ConnectCallback cb) = 0;
    /**
     * @brief 创建音视频客户端通道
     * @return
     */
    virtual IMediaBusClient::Ptr createMediaBusClient() = 0;
    /**
     * @brief 注册运控观测量回调（observed 上报开启后随帧推送，含 power）
     */
    virtual void setMotionObservedCallback(MotionObservedCallback cb) = 0;
    /**
     * @brief 获取电池电量信息
     * @param power
     * @param timeout
     * @return
     */
    virtual bool getPowerInfo(PowerObserved *power, uint32_t timeout) = 0;
public:
    /**
     * @brief 停止播放（必须先 startControl），对应高层 RPC stopPlayList，空参形态
     */
    virtual bool stopAudioPlay(uint32_t timeout = 5000) = 0;
    /**
     * @brief 暂停播放（必须先 startControl），对应高层 RPC stopPlayList + {"pause":true}
     *        恢复播放请用 startAudioPlay("{\"resume\":true}", ...)
     */
    virtual bool pauseAudioPlay(uint32_t timeout = 5000) = 0;
    /**
     * @brief 查询当前播放详情（任何已 connect 状态均可），对应高层 RPC getAudioPlayDetail
     */
    virtual bool queryAudioPlayDetail(std::string& out, uint32_t timeout = 5000) = 0;
    /**
     * @brief 获取摄像头补光灯亮度
     * @param out
     * @param timeout
     * @return
     */
    virtual bool getCameraLightBrightness(std::string& out, uint32_t timeout = 5000) = 0;
    /**
     * @brief 摄像头前灯亮度控制（必须先 startControl）
     *        对应 RPC robotAppService.setFrontLightBrightness
     * @param brightness 亮度 0~100
     */
    virtual bool setCameraLightBrightness(int32_t brightness, uint32_t timeout = 5000) = 0;
    /**
     * @brief 启动/调参/恢复/改重复次数 —— 复用高层 RPC startPlayList，按 paramsJson 字段决定语义
     */
    virtual bool startAudioPlay(const std::string& paramsJson, uint32_t timeout = 5000) = 0;
    /**
     * @brief 新增音频文件（必须先 startControl），对应高层 RPC addAudioFile
     */
    virtual bool addAudioFile(const std::string& paramsJson, uint32_t timeout = 30000) = 0;
    /**
     * @brief 删除音频文件（必须先 startControl），对应高层 RPC deleteAudioFile
     */
    virtual bool deleteAudioFile(const std::string& paramsJson, uint32_t timeout = 5000) = 0;
    /**
     * @brief 查询音频文件列表（任何已 connect 状态均可），对应高层 RPC getAudioPlayList
     */
    virtual bool queryAudioPlayList(std::string& out, const std::string& paramsJson = "", uint32_t timeout = 5000) = 0;
public:
    /**
     * @brief 停止当前动作
     */
    virtual bool stopAction(uint32_t timeout = 5000) = 0;
    /**
     * @brief 发送原始控制指令
     * @param frame  原始控制指令
     * @return
     */
    virtual bool setRawControlCmd(TRCStickFrame &frame) = 0;
    /**
     * @brief 异步请求控制权（立即返回，state 由 worker 推进至 kControlled）
     * @param timeout 拿到控制权的整体截止时间（ms）。worker 在此时间内持续重试 acquireMode；
     *                超时仍未拿到 → 撤销 intent + ConnectCallback(kConnected, kRpcAcquireRejected)
     * @return true 请求已受理（不代表已持有，调用方通过 ConnectCallback / getState() 感知）；
     *              false 当前 kDisconnected（必须先 connect）
     */
    virtual bool startControl(uint32_t timeout = 10000) = 0;
    /**
     * @brief 急停（必须先 startControl 获取控制权）
     */
    virtual bool emergencyStop(uint32_t timeout = 5000) = 0;
    /**
     * @brief 跌倒后恢复站立（自我翻正 + 起立）
     */
    virtual bool recoveryStand(uint32_t timeout = 5000) = 0;
    /**
     * @brief 查询当前运控状态（与控制权无关，任何已 connect 状态均可）
     * @param out 输出 JSON 字符串（UTF-8）；有活动动作时字段：
     *   - action        string 当前动作名，如 "walking"
     *   - velocity      float  角速度/转向速度（单位见 queryCapabilities 对应 param 的 unit）
     *   - lineVelocityX float  前后方向线速度，正值向前
     *   - lineVelocityY float  左右方向线速度，正值方向由设备配置定义
     * @return true = RPC 成功；**无活动动作时 out 是空对象 `{}`**，调用方据此判断"无可查询动作"，不要假设字段必定存在
     *         false = RPC 失败（未 connect / 超时 / 通道不可用等）；通过 getLastError() 取错误码
     */
    virtual bool queryMotionState(std::string& out, uint32_t timeout = 5000) = 0;
    /**
     * @brief 查询系统状态详情（任何已 connect 状态均可），对应 RPC robotAppService.getStatusDetail
     * @param out 输出 JSON 字符串（UTF-8），字段：
     *   - network object  网络信息：ether/hotspot/mobile/wlan 子对象，每个含
     */
    virtual bool querySystemStatus(std::string& out, uint32_t timeout = 5000) = 0;
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
     * @brief 查询服务端能力（支持的高级动作集合等，与控制权无关）
     * @param out 输出 JSON 字符串（UTF-8），数组元素字段：
     *   - name    string 动作名
     *   - buttons array  动作对应的 TRC 按键组合，仅用于诊断/展示
     *   - params  array  可选动作参数，元素含 name/min/max/unit
     */
    virtual bool getMotionCapabilities(std::string& out, uint32_t timeout = 5000) = 0;
    /**
     * @brief 修改当前动作参数（不切换动作）
     * @param paramsJson 动作参数 JSON 字符串。字段以 queryCapabilities() 返回的 params 为准；
     *                   当前通用速度字段包括 velocity、lineVelocityX、lineVelocityY。
     *                   例如 walking 前进：{"lineVelocityX":1.5}
     * @note 参数会转换为 TRC 轴值并保持生效；再次调用 startAction/setActionParams 或 stopAction 会覆盖/清除。
     */
    virtual bool setActionParams(const std::string& paramsJson = "", uint32_t timeout = 5000) = 0;
    /**
     * @brief 观测数据上报（任何已 connect 状态均可），对应 RPC robotAppService.setMotionObservedEnable
     *        服务端 hook 不做鉴权，SDK 侧也不强校验持权。
     * @param json 设置配置
     * @param ret  当前配置
     */
    virtual bool setObservedEnable(const std::string& json,std::string& ret,uint32_t timeout = 5000) = 0;
    /**
     * @brief 启动执行高级动作（必须先 startControl）
     * @param action 动作名（如 "walking" / "jumpFrontflip" / "jumpBackflip"）
     * @param paramsJson 动作参数 JSON 字符串。字段以 queryCapabilities() 返回的 params 为准；
     *                   walking 常用字段：velocity、lineVelocityX、lineVelocityY。
     *                   不带速度参数时只切入动作姿态，不会自动向前移动。
     * @note 单次 startAction 会把动作和参数写入服务端 TRC 状态缓存，持续生效直到 stopAction
     *       或后续动作/参数调用覆盖；不需要周期性重复发送同一 walking 请求。
     */
    virtual bool startAction(const std::string& action, const std::string& paramsJson = "", uint32_t timeout = 5000) = 0;
public:
    /**
     * @brief 进入阻尼/慢沉（软卸力）
     * @note 关节切到低刚度阻尼，在重力下缓慢可控下沉，不锁死也不瘫砸；用作安全软停
     */
    virtual bool damp(uint32_t timeout = 5000) = 0;
    /**
     * @brief 趴下/卧倒
     */
    virtual bool lieDown(uint32_t timeout = 5000) = 0;
    /**
     * @brief 站立
     */
    virtual bool standUp(uint32_t timeout = 5000) = 0;
    /**
     * @brief 行走
     * @param vx   前后线速度，正值向前（单位见 queryCapabilities 对应 param）
     * @param vy   左右线速度（正负方向由设备配置定义）
     * @param vyaw 转向角速度
     * @note 持续生效直到 stopAction 或后续动作/参数覆盖；仅调速度用 setActionParams 即可，无需重复 move
     */
    virtual bool move(float vx, float vy, float vyaw, uint32_t timeout = 5000) = 0;
};

}
}
#endif //ROBOTSERVICE_MOTIONHIGHLEVELCLIENT_H
