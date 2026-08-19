#pragma once

#include "PkStream.h"
#include "PkString.h"

// PkFileStream —— PkStream 的文件适配器，对应 Qt 的文件设备类。
//
// S-01 参考适配器：包 POSIX open/read/write/lseek/close，把 PkStream 的模板
// 方法（公开 read()/write() 转发 + 受保护纯虚 readData()/writeData()）接到
// 真实文件上。Task 5 的 KoDirectoryStore 消费（替代其文件设备用法）。
//
// 语义契约来自 PkStream.h 头注释（R-12 探针实测，硬约束）：
//   - EOF 返回 0；-1 只在设备未 open 时出现；
//   - 短读不补零：read()/write() 各转发一次 readData()/writeData()，对方给
//     多少就返回多少；
//   - isSequential()==false（真 Qt 的文件设备是非顺序设备），pos()/atEnd() 用
//     基类实现——基类在非顺序设备上自行推进内部游标 m_pos，size() 是虚函数，
//     这里 override 成真实文件大小，atEnd()==bytesAvailable()==0==size-pos 自洽。
//
// 头文件必须 #include "PkString.h" 拿完整定义：m_filePath 按值存储，不能只前置
// 声明 PkString。
class PkFileStream : public PkStream
{
public:
    PkFileStream();
    explicit PkFileStream(const PkString &filePath);
    explicit PkFileStream(const char *filePath);
    ~PkFileStream() override;

    PkString fileName() const;

    // 真打开底层文件（基类默认实现是 setOpenMode(mode); return true;，不真开）。
    bool open(OpenMode mode) override;
    void close() override;

    // 原始 POSIX write() 无用户态缓冲，数据直接进内核，flush 恒成功。
    bool flush();

    pk_int64 size() const override;
    bool seek(pk_int64 pos) override;
    bool isSequential() const override;   // 恒 false

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override;
    pk_int64 writeData(const char *data, pk_int64 maxSize) override;

private:
    PkString m_filePath;
    int m_fd;                             // -1 = 未打开
};
