/************************************************
 * Copyright(c) 2019 Sang Yang
 * 
 * Project:  framework
 * FileName: Packet.h
 * Author: xieshren
 * Email: xieshren@gmail.com
 * Version: V1.0.0
 * Date: 2020-01-07 21:35
 * Description: 
 * Others:
 *************************************************/
#ifndef FRAMEWORK_INCLUDE_MEMORY_PACKET_H
#define FRAMEWORK_INCLUDE_MEMORY_PACKET_H

#include "uniubi/robot_sdk/UBase/Define.h"
#include "uniubi/robot_sdk/UBase/Delegate.h"

namespace Uface {
namespace Memory {

/**
 * @brief 自定义内存释放函数指针
 */
typedef UBase::TDelegate3<void,void*,size_t,uint8_t*> FreeMemoryFunc;

class MemoryInternal;

/**
 * @brief  内存包类定义
 */
class EXPORT_API Packet {

public:
    /**
     * @brief 构造函数
     * @note 生成无效的包，需要赋值才能使用
     */
    Packet();
    /**
     * @brief 构造函数
     * @param[in] data      数据指针
     * @param[in] length    数据长度
     * @param[in] func      data数据释放函数(默认为空不释放)
     */
    Packet(void* data, int32_t length, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 构造函数
     * @param data
     * @param length
     * @param extraSize
     * @param func
     */
    Packet(void* data, int32_t length, int32_t extraSize, FreeMemoryFunc func = FreeMemoryFunc());
    /**
     * @brief 拷贝构造函数
     * @param[in] packet 完成包引用计数递增
     */
    Packet(const Packet& packet);
    /**
     * @brief 大包中构造小包,小包引用大包的局部内存
     * @note  如果参数不正确，则构造一个无效的包
     * @param[in] packet    被引用的大包
     * @param[in] offset    被引用的内存在大包中的内存偏移字节数
     * @param[in] size      被引用的内存字节数
     * @param[in] extraSize
     */
    Packet(const Packet& packet, int32_t offset, int32_t size, int32_t extraSize);
    /**
     * @brief 构造函数,生成一个可以使用的包
     * @param[in] dataSize  申请的包中数据缓冲区的字节数
     * @param[in] extraSize 扩展数据缓冲区字节数
     */
    explicit Packet(int32_t dataSize, int32_t extraSize = 0);
    /**
     * @brief 构造函数
     * @param[in] size      申请的包中数据缓冲区的字节数
     * @param[in] func      用自定义释放参数指针
     * @param[in] arg       用户自定参数指针
     * @param[in] extraSize 扩展数据缓冲区字节数
     */
    explicit Packet(int32_t size, FreeMemoryFunc func, void* arg, int32_t extraSize = 0);
    /**
     * @brief 赋值操作
     * @param[in] packet  被赋值对象
     * @return
     */
    Packet& operator=(const Packet& packet);
    /**
     * @brief 析构函数
     */
    virtual ~Packet();

public:
    /**
     * @brief 包重置，包恢复为无效包
     */
    void reset();
    /**
     * @brief 判断包是否有效
     * @return
     */
    bool valid() const;
    /**
     * @brief 向当前包缓冲的指针偏移处追加数据,内部自动更新数据长度
     * @param[in] data      追加的数据指针
     * @param[in] length    追加的数据长度
     * @return  实际写入的数据, < length: 缓冲区已满 剩余数据需要调用者自行处理
     */
    int32_t putBuffer(void* data,int32_t length);
    /**
     * @brief 获取有效数据长度
     * @return
     */
    int32_t size() const;
    /**
     * @brief 获取包缓冲区起始位置指针
     * @return
     */
    uint8_t* getBuffer() const;
    /**
     * @brief 设置有效数据长度
     * @param[in] size  新的长度，不会超过包的容量
     * @return
     */
    bool resize(size_t size);
    /**
     * @brief 获取容量
     * @return
     */
    int32_t capacity() const;
    /**
     * @brief 获取扩展头指针
     * @return
     */
    void* getExtraData() const;
    /**
     * @brief 获取扩展数据长度
     * @return
     */
    int32_t getExtraSize() const;

private:
    MemoryInternal*     mInternal;
};

}
}
#endif //FRAMEWORK_INCLUDE_MEMORY_PACKET_H
