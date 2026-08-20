/*
 *  MD5.h —— 无外部框架依赖的 MD5 摘要实现。
 *
 *  算法逐字对照 RFC 1321 参考实现（公共领域，Colin Plumb 1993 首版，
 *  后由 Alexander Peslyak 等广泛传播），C++ 类封装。输出为 16 字节摘要或
 *  小写 hex 字符串。
 *
 *  S-02-b 拉前依赖：KoMD5Generator 需要。本文件随 KoMD5Generator 一起引入。
 */
#ifndef MD5_H
#define MD5_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

class MD5
{
public:
    using Digest = std::array<std::uint8_t, 16>;

    MD5();

    // 追加输入。可多次调用；终结后再 addData 会自动 reset。
    void addData(const void *data, std::size_t length);
    void addData(const char *data, std::size_t length);

    // 回到初始状态。
    void reset();

    // 终结并返回 16 字节摘要。可重复调用，结果缓存。
    Digest digest();

    // 返回小写 hex 摘要（16 字节 → 32 字符）。可重复调用，结果缓存。
    std::string toHex();

private:
    void processBlock(const std::uint8_t *block);

    std::uint32_t m_state[4];    // A/B/C/D
    std::uint64_t m_bitCount;    // 已处理总位数（不含当前缓冲）
    std::uint8_t m_buffer[64];   // 待处理块缓冲
    std::size_t m_bufferLen;
    bool m_finalized;
    Digest m_digest;
    std::string m_hex;
};

#endif // MD5_H
