#pragma once

#include <string>

// PkByteArray 归 R-02 交付；本端口只前置声明（见下方「刻意的设计」）。
class PkByteArray;
// PkString 归 R-01（pk/string），已交付。这里同样只前置声明——errorString()/
// setErrorString() 在 .cpp 里才需要它的完整定义，头文件不该为一个返回值类型
// 拖出整条 include 链。
class PkString;

// PkStream —— 零 Qt 依赖的字节流端口，对应 Qt 的 QIODevice。
//
// R-12 只出这一层接口 + 基类模板逻辑，不出任何具体适配器（文件/内存/zip 归 S-01
// 等后续批次）。保留范围内目前唯一的 QIODevice 子类是 KoStoreDevice，它
// override 的正是 readData()/writeData() 并调用 setOpenMode()——这是本类把
// read()/write() 设计成「公开转发层 + 受保护纯虚 readData()/writeData()」这个
// 模板方法拆分必须原样保留的唯一理由：拆分没了，KoStoreDevice 的改法就不是
// 「零改动」了。
//
// ── 语义对齐真 Qt：以下全部来自 pk/port/probe/probe_qiodevice.cpp 实测
//    （真链 Qt 5.15.13），不是猜的。跑法见 pk/port/probe/README.md。────────
//
// 1. **EOF 不是错误**：read() 到设备末尾返回 0；返回 -1 只发生在设备根本没
//    open() 过的时候。errorString()/error() 在 EOF 之后仍然是「无错误」。
//
// 2. **短读不补零**：一次 read()/write() 调用只转发一次 readData()/writeData()，
//    对方能给多少就返回多少，不在这一层循环重试去凑满 maxSize。
//
// 3. **顺序设备（isSequential()==true）上 pos() 恒为 0**——本基类**不为它推进
//    内部游标**。这条是硬契约，写给所有子类：
//
//        isSequential()==true 时，readData() 的实现必须自己维护「读到哪了」
//        这件事（例如子类自己存一个私有游标/管道已消费字节数），绝不能用
//        pos() 去索引底层数据。
//
//    原因：探针踩过一次真事故——把一个内存设备的 isSequential() 改成 true
//    之后，readAll() 死循环，一口气吐了 64MB。根因是该设备的 readData() 用
//    pos() 当偏移量索引自己的缓冲区，而 pos() 对顺序设备永远是 0，于是每次
//    都从第 0 字节重新发送，永远不到 EOF。这个类的 read()/ungetChar() 等只在
//    !isSequential() 时才会挪动内部游标，isSequential()==true 时游标原地不动
//    ——子类必须自带一份真正会前进的游标，不能依赖基类这份。
//
// ── 一个刻意的设计，不要「修正」它 ──────────────────────────────────────
//
// readAll() 由本端口实现，使用 read() 的短读、EOF、错误和顺序设备语义；
// PkByteArray 已由 R-02 交付。peek()/readLine() 的 PkByteArray 重载仍由后续任务
// 实现。
class PkStream
{
public:
    enum OpenModeFlag {          // 位值照抄真 Qt QIODevice::OpenModeFlag
        NotOpen = 0x0000, ReadOnly = 0x0001, WriteOnly = 0x0002,
        ReadWrite = 0x0003, Append = 0x0004, Truncate = 0x0008,
        Text = 0x0010, Unbuffered = 0x0020, NewOnly = 0x0040, ExistingOnly = 0x0080
    };
    typedef unsigned int OpenMode;
    typedef long long pk_int64;

    PkStream();
    virtual ~PkStream();

    virtual bool     open(OpenMode mode);
    virtual void     close();
    bool             isOpen() const;
    OpenMode         openMode() const;
    bool             isReadable() const;
    bool             isWritable() const;

    virtual bool     isSequential() const;
    virtual pk_int64 size() const;
    virtual pk_int64 pos() const;
    virtual bool     seek(pk_int64 pos);
    virtual bool     atEnd() const;
    virtual pk_int64 bytesAvailable() const;
    virtual bool     canReadLine() const;

    pk_int64 read(char *data, pk_int64 maxSize);
    pk_int64 peek(char *data, pk_int64 maxSize);
    pk_int64 skip(pk_int64 maxSize);
    pk_int64 readLine(char *data, pk_int64 maxSize);
    pk_int64 write(const char *data, pk_int64 maxSize);
    bool     getChar(char *c);
    bool     putChar(char c);
    void     ungetChar(char c);
    PkString errorString() const;

    PkByteArray readAll();
    PkByteArray peek(pk_int64 maxSize);
    PkByteArray readLine();

protected:
    virtual pk_int64 readData(char *data, pk_int64 maxSize) = 0;
    virtual pk_int64 writeData(const char *data, pk_int64 maxSize) = 0;
    void setOpenMode(OpenMode mode);
    void setErrorString(const PkString &str);

private:
    OpenMode m_openMode;
    pk_int64 m_pos;
    // ungetChar() 注入的字节，按「先进先出」的顺序等待被下一次 read()/peek()
    // 取走——front() 是下一个要被读到的字节。peek() 就是靠「read() 之后把读到
    // 的字节原样 ungetChar() 回去」实现的，不需要另一套缓冲逻辑。
    std::string m_ungetBuffer;
    // 内部只存 UTF-8 字节，避免头文件为 errorString() 这一个返回值类型拖出
    // PkString 的完整定义。空字符串代表「没设过」，errorString() 对外返回时
    // 补成 "Unknown error"（真 Qt QIODevice 的默认文案，探针 Follow-up A 确认
    // EOF、只读设备被拒绝写入之后这个值仍然不变，只是没有实测过它的初始值到
    // 底是不是这四个词——这一条按「已知的 Qt 默认」处理，不是探针条目①-⑧
    // 覆盖的范围，见任务报告「哪些地方是你猜的」）。
    std::string m_errorString;
};
