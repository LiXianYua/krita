#include "PkMemoryStream.h"

#include <cstring>

PkMemoryStream::PkMemoryStream() = default;

PkMemoryStream::~PkMemoryStream() = default;

const char *PkMemoryStream::data() const
{
    return m_buffer.data();
}

PkStream::pk_int64 PkMemoryStream::size() const
{
    return static_cast<pk_int64>(m_buffer.size());
}

bool PkMemoryStream::isSequential() const
{
    return false;
}

PkStream::pk_int64 PkMemoryStream::readData(char *data, pk_int64 maxSize)
{
    const pk_int64 p = pos();
    if (p < 0) {
        return -1;
    }
    // 游标在缓冲末尾或之后：EOF，返回 0——契约第 1 条「EOF 不是错误」。
    if (static_cast<size_t>(p) >= m_buffer.size()) {
        return 0;
    }
    const pk_int64 avail = static_cast<pk_int64>(m_buffer.size()) - p;
    const pk_int64 n = maxSize < avail ? maxSize : avail;
    if (n > 0) {
        std::memcpy(data, m_buffer.data() + p, static_cast<size_t>(n));
    }
    // 短读返回实际字节数，不补零（契约第 2 条）。
    return n;
}

PkStream::pk_int64 PkMemoryStream::writeData(const char *data, pk_int64 maxSize)
{
    const pk_int64 p = pos();
    if (p < 0) {
        return -1;
    }
    // 内存缓冲语义（对应 Qt 的内存缓冲类）：写到当前游标位置，越界（游标超出
    // 缓冲末尾）则补零扩展到 p+maxSize 再写。基类 write() 在返回后会把 m_pos
    // 推进 maxSize，与这里写入的区间自洽。
    const size_t need = static_cast<size_t>(p + maxSize);
    if (m_buffer.size() < need) {
        m_buffer.resize(need, '\0');
    }
    std::memcpy(m_buffer.data() + p, data, static_cast<size_t>(maxSize));
    return maxSize;
}
