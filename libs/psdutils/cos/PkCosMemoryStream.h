#pragma once

#include "PkStream.h"
#include "kritapsdutils_export.h"

class PkByteArray;

/**
 * @brief PkCosMemoryStream —— psdutils 用的 PkStream 内存流（对应 Qt 的 QBuffer）。
 *
 * 包一个外部 PkByteArray（构造时持有指针，不转移所有权），isSequential()==false，
 * pos()/seek() 生效。COS 解析/序列化用：parser 以 ReadOnly 打开读，writer 以
 * WriteOnly 打开写（写越界补零扩展缓冲，与 Qt 内存缓冲语义一致）。
 *
 * 语义对齐 PkMemoryStream（libs/store）：
 *   - readData()：游标 p<0 → -1；p>=缓冲末尾 → 0（EOF 不是错误）；否则
 *     memcpy min(maxSize, avail)，短读不补零。
 *   - writeData()：缓冲不足则补零扩展到 p+maxSize，再 memcpy，返回 maxSize。
 *
 * 这是本任务（S-03-d Task 1 剥离 cos trio）的实现产物，不是端口：PkStream 是
 * 抽象基类（readData/writeData 纯虚），无现成内存流实现，psdutils 内需要自己的
 * QBuffer 对应物。
 */
class KRITAPSDUTILS_EXPORT PkCosMemoryStream : public PkStream
{
public:
    explicit PkCosMemoryStream(PkByteArray *ba);
    ~PkCosMemoryStream() override;

    bool isSequential() const override;   // 恒 false
    pk_int64 size() const override;

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override;
    pk_int64 writeData(const char *data, pk_int64 maxSize) override;

private:
    PkByteArray *m_ba;
};
