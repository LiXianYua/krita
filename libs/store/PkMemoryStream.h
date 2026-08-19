#pragma once

#include "PkStream.h"

#include <vector>

// PkMemoryStream —— PkStream 的内存适配器，对应 Qt 的内存缓冲类。
//
// S-01 参考适配器：内部 std::vector<char> 缓冲，游标直接用基类 pos()——
// isSequential()==false 时基类会自行推进内部 m_pos，readData()/writeData()
// 用 pos() 索引缓冲即可，不在子类另存一份游标（PkStream.h 头注释第 3 条
// 「顺序设备上 pos() 恒为 0」那条只约束 isSequential()==true 的子类，本类
// 返回 false，pos() 可用）。Task 6 的 KoQuaZipStore 内存缓冲缓存与
// KoStore::extractFile 消费。
//
// 语义契约（PkStream.h 头注释）：EOF 返回 0、短读不补零、isSequential()==false。
//
// 对外暴露只读 data()/size()：KoStore 要把内存缓冲构造为 PkByteArray——
// PkByteArray 归 R-02 未交付，本类不改它，只把 data/size 接口留出来。
class PkMemoryStream : public PkStream
{
public:
    PkMemoryStream();
    ~PkMemoryStream() override;

    // 只读访问缓冲内容；缓冲为空时可能返回 nullptr（std::vector 语义）。
    const char *data() const;
    pk_int64 size() const override;

    bool isSequential() const override;   // 恒 false

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override;
    pk_int64 writeData(const char *data, pk_int64 maxSize) override;

private:
    std::vector<char> m_buffer;
};
