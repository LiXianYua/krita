/*
 *  MD5.h —— 零 Qt 依赖的 MD5 摘要实现（vendored，替换 QCryptographicHash）。
 *
 *  算法逐字对照 RFC 1321 参考实现（公共领域，Colin Plumb 1993 首版，
 *  后由 Alexander Peslyak 等广泛传播），C++ 类封装。输出为小写 hex 字符串，
 *  与 Qt QCryptographicHash::result().toHex() 一致。
 *
 *  S-02-b 拉前依赖：KoMD5Generator 需要（impact-map.md §4「QCryptographicHash →
 *  vendored MD5」，计划文档 plan §Task 5）。本文件随 KoMD5Generator 一起在本
 *  Task 引入。
 */
#ifndef MD5_H
#define MD5_H

#include <cstddef>
#include <cstdint>
#include <string>

class MD5
{
public:
    MD5();

    // 追加输入。可多次调用；finalize（toHex）后再 addData 会自动 reset。
    void addData(const void *data, std::size_t length);
    void addData(const char *data, std::size_t length);

    // 回到初始状态。
    void reset();

    // 终结并返回小写 hex 摘要（16 字节 → 32 字符）。可重复调用，结果缓存。
    std::string toHex();

private:
    void processBlock(const std::uint8_t *block);

    std::uint32_t m_state[4];    // A/B/C/D
    std::uint64_t m_bitCount;    // 已处理总位数（不含当前缓冲）
    std::uint8_t m_buffer[64];   // 待处理块缓冲
    std::size_t m_bufferLen;
    bool m_finalized;
    std::string m_hex;
};

#endif // MD5_H
