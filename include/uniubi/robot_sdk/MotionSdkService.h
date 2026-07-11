/**
 * ========================================================
 *  @file MotionSdkService.h
 *  @brief  Motion SDK 全局入口（对外接口，单例）
 *  @author shangyang
 *  @date 2026-05-07
 *  @version 1.0
 *  @details
 *      负责 SDK 共享资源的全局初始化与释放：
 *        - JSON 配置加载与节点查询
 *        - UDBus DDS 工厂（RPC over DDS 的运行基础）
 *        - 单 RPC 客户端（call / sendAction）
 *        - 客户端标识（用于 RPC call.clientId）
 *      App 使用：
 *        IMotionSdkService::instance()->initialService("config.json", "motionSdk");
 *  @copyright Copyright (c) 2026 UNIUBI All rights reserved.
 * ========================================================
 */

#ifndef ROBOTSERVICE_MOTIONSDKSERVICE_H
#define ROBOTSERVICE_MOTIONSDKSERVICE_H

#include <string>
#include <cstring>
#include <cstdint>
#include <functional>

namespace uniubi {
namespace RobotSdk {

class IMotionSdkService {

public:

    typedef enum {
        kLogDebug = 0,
        kLogTrace,
        kLogInfo,
        kLogWarn,
        kLogError,
        kLogFatal,
    } LogLevel;
    /**
     * @brief SDK 日志回调；msg 不带换行，length 为有效长度
     */
    using LogCallback = std::function<void(LogLevel level, const char* msg, int32_t length)>;
    /**
     * @brief 设备响应回调：发现期间每收到一台机器人响应即触发一次
     * @param sn   机器人 SN
     * @param info 设备详情 JSON 字符串（调用方自行 parse），典型字段：
     *               - version       : 整机软件版本
     *               - brainVersion  : 大脑（高层算法）版本
     *               - deviceCP      : 主控芯片标识
     *               - deviceModel   : 设备型号
     *               - productDate   : 出厂日期
     *               - network       : 网络状态（同 querySystemStatus.network，含 ether/hotspot/mobile/wlan）
     *             具体字段以设备版本下发为准；客户端遇未知字段宽容透传。
     */
    using DeviceDiscover = std::function<void(const std::string& sn, const std::string& info)>;
public:
    /**
     * @brief 获取 SDK 版本字符串
     * @details 格式："<semver> (commit <git-short-sha>)"，例如 "0.1.0 (commit 7bb376b2)"。
     *          任意时刻可调，无需先 initialService；返回字符串由 SDK 静态持有，调用方不应释放
     */
    static const char* version();
    /**
     * @brief 单例入口（实际实例由 MotionBusServiceImpl 提供）
     */
    static IMotionSdkService* instance();
public:
    /**
     * @brief 释放共享资源 + 清空配置
     */
    virtual void shutdown() = 0;
    /**
     * @brief 查询当前部署是否支持多设备
     * @details 单设备场景：调用方无需调 discoverDevices，直接 `create()` 即可
     *          多设备场景：调用方应先 setDiscoverCallback + discoverDevices 拿到 SN 后再 `create(sn)`
     */
    virtual bool isMultiDevice() const = 0;
    /**
     * @brief 注册日志回调；传 nullptr 取消注册
     */
    virtual void setLogCallback(LogCallback cb) = 0;
    /**
     * @brief 指定 SDK 使用的网络接口（多设备/远端模式生效，板内部署忽略）
     * @details 例如 "eth0" / "wlan0"，必须在 initialService 之前调；
     *          未指定时由 Cyclone DDS 自动选择接口
     */
    virtual void setNetworkInterface(const char* iface) = 0;
    /**
     * @brief 设置 设备发现回调
     * @param cb
     */
    virtual void setDiscoverCallback(DeviceDiscover cb) = 0;
    /**
     * @brief 主动发起一次设备发现（非阻塞，立即返回）
     * @details 内部 publish 询问事件并开启/延长一个发现窗口后立即返回，不等待响应；
     *          窗口期内每台机器人的响应通过 setDiscoverCallback 注册的回调异步触发
     * @param timeoutMs 发现窗口时长(ms)；已有未过期窗口则延长至至少该时长
     */
    virtual bool discoverDevices(uint32_t timeoutMs = 10000) = 0;
    /**
     * @brief 全局初始化：加载配置客户端
     * @param file    JSON 配置文件路径
     * @param server  SDK 客户端标识（用于 RPC call.clientId）
     * @param timeout 等待系统环境准备好；本地模式下，如果sdk启动比系统环境早，那么可能超时; 单位s
     */
    virtual bool initialService(const char* file, const char* server,uint32_t timeout = 30) = 0;
};

}
}
#endif //ROBOTSERVICE_MOTIONSDKSERVICE_H
