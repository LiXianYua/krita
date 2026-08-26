#include "PkCosMemoryStream.h"

#include <PkAuxTypes.h>

#include <cstring>

PkCosMemoryStream::PkCosMemoryStream(PkByteArray *ba)
    : m_ba(ba)
{
}

PkCosMemoryStream::~PkCosMemoryStream() = default;

bool PkCosMemoryStream::isSequential() const
{
    return false;
}

PkStream::pk_int64 PkCosMemoryStream::size() const
{
    return m_ba ? static_cast<pk_int64>(m_ba->size()) : 0;
}

PkStream::pk_int64 PkCosMemoryStream::readData(char *data, pk_int64 maxSize)
{
    const pk_int64 p = pos();
    if (p < 0) {
        return -1;
    }
    const pk_int64 sz = size();
    // 游标在缓冲末尾或之后：EOF，返回 0——契约「EOF 不是错误」。
    if (p >= sz) {
        return 0;
    }
    const pk_int64 avail = sz - p;
    const pk_int64 n = maxSize < avail ? maxSize : avail;
    if (n > 0) {
        std::memcpy(data, m_ba->constData() + p, static_cast<size_t>(n));
    }
    // 短读返回实际字节数，不补零。
    return n;
}

PkStream::pk_int64 PkCosMemoryStream::writeData(const char *data, pk_int64 maxSize)
{
    const pk_int64 p = pos();
    if (p < 0) {
        return -1;
    }
    // 内存缓冲语义（对应 Qt 的内存缓冲类）：写到当前游标位置，越界（游标超出
    // 缓冲末尾）则补零扩展到 p+maxSize 再写。基类 write() 在返回后会把 m_pos
    // 推进 maxSize，与这里写入的区间自洽。
    const size_t need = static_cast<size_t>(p + maxSize);
    if (static_cast<size_t>(m_ba->size()) < need) {
        m_ba->resize(static_cast<int>(need));
    }
    std::memcpy(m_ba->data() + p, data, static_cast<size_t>(maxSize));
    return maxSize;
}
