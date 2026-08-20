/*
 *  MD5.cpp —— 零 Qt 依赖的 MD5 摘要实现（vendored）。
 *  算法对照 RFC 1321 参考实现；仅依赖 <cstring>。
 */
#include "MD5.h"

#include <cstring>

namespace {

// 四轮循环左移位数（RFC 1321 表 T）。
const int kShift[64] = {
    7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22,
    5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
    4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23,
    6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21
};

// 64 个常数 K[i] = floor(|sin(i+1)| * 2^32)（RFC 1321 表 K）。
const std::uint32_t kK[64] = {
    0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
    0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
    0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
    0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
    0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
    0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
    0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
    0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391
};

inline std::uint32_t rotl(std::uint32_t x, int s)
{
    return (x << s) | (x >> (32 - s));
}

} // namespace

MD5::MD5()
{
    reset();
}

void MD5::reset()
{
    m_state[0] = 0x67452301;
    m_state[1] = 0xefcdab89;
    m_state[2] = 0x98badcfe;
    m_state[3] = 0x10325476;
    m_bitCount = 0;
    m_bufferLen = 0;
    m_finalized = false;
    m_digest.fill(0);
    m_hex.clear();
}

void MD5::addData(const void *data, std::size_t length)
{
    if (m_finalized) {
        reset();
    }
    const unsigned char *bytes = static_cast<const unsigned char *>(data);
    m_bitCount += static_cast<std::uint64_t>(length) * 8;

    while (length > 0) {
        const std::size_t room = 64 - m_bufferLen;
        const std::size_t take = length < room ? length : room;
        std::memcpy(m_buffer + m_bufferLen, bytes, take);
        m_bufferLen += take;
        bytes += take;
        length -= take;
        if (m_bufferLen == 64) {
            processBlock(m_buffer);
            m_bufferLen = 0;
        }
    }
}

void MD5::addData(const char *data, std::size_t length)
{
    addData(static_cast<const void *>(data), length);
}

void MD5::processBlock(const std::uint8_t *block)
{
    std::uint32_t M[16];
    for (int i = 0; i < 16; ++i) {
        M[i] = static_cast<std::uint32_t>(block[i * 4])
            | (static_cast<std::uint32_t>(block[i * 4 + 1]) << 8)
            | (static_cast<std::uint32_t>(block[i * 4 + 2]) << 16)
            | (static_cast<std::uint32_t>(block[i * 4 + 3]) << 24);
    }

    std::uint32_t a = m_state[0];
    std::uint32_t b = m_state[1];
    std::uint32_t c = m_state[2];
    std::uint32_t d = m_state[3];

    for (int i = 0; i < 64; ++i) {
        std::uint32_t f;
        int g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) & 15;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) & 15;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) & 15;
        }

        const std::uint32_t tmp = d;
        d = c;
        c = b;
        b = b + rotl(a + f + kK[i] + M[g], kShift[i]);
        a = tmp;
    }

    m_state[0] += a;
    m_state[1] += b;
    m_state[2] += c;
    m_state[3] += d;
}

MD5::Digest MD5::digest()
{
    if (!m_finalized) {
        // 填充：先补 0x80，再补零到长度 ≡ 56 (mod 64)，最后 64 位小端长度。
        const std::uint64_t bitLen = m_bitCount;
        static const unsigned char pad0x80 = 0x80;
        addData(&pad0x80, 1);
        while (m_bufferLen != 56) {
            static const unsigned char zero = 0;
            addData(&zero, 1);
        }
        unsigned char lenBytes[8];
        for (int i = 0; i < 8; ++i) {
            lenBytes[i] = static_cast<unsigned char>((bitLen >> (8 * i)) & 0xff);
        }
        addData(lenBytes, 8);

        std::size_t digestIndex = 0;
        for (int i = 0; i < 4; ++i) {
            for (int j = 0; j < 4; ++j) {
                m_digest[digestIndex++] =
                    static_cast<std::uint8_t>((m_state[i] >> (8 * j)) & 0xff);
            }
        }

        static const char hexDigits[] = "0123456789abcdef";
        m_hex.reserve(32);
        for (const std::uint8_t byte : m_digest) {
            m_hex.push_back(hexDigits[byte >> 4]);
            m_hex.push_back(hexDigits[byte & 0x0f]);
        }
        m_finalized = true;
    }
    return m_digest;
}

std::string MD5::toHex()
{
    digest();
    return m_hex;
}
