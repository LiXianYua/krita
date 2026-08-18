#include "PkString.h"

#include "PkStringCodec.h"

#include <cstring>
#include <utility>

PkString::PkString() = default;   // PkArrayData() 默认构造：零分配，指向共享空哨兵

PkString::PkString(const char* utf8)
    : _d(PkStringCodec::FromUtf8(utf8, utf8 != nullptr ? std::strlen(utf8) : 0))
{
}

PkString::PkString(const PkString& other) = default;
PkString::PkString(PkString&& other) noexcept = default;
PkString::~PkString() = default;
PkString& PkString::operator=(const PkString& other) = default;
PkString& PkString::operator=(PkString&& other) noexcept = default;

const std::vector<char16_t>& PkString::_cbuf() const
{
    return _d.PkConst();
}

const char16_t* PkString::_cdata() const
{
    return _cbuf().data();
}

std::vector<char16_t>& PkString::_data()
{
    return _d.PkMut();
}

int PkString::size() const
{
    return static_cast<int>(_cbuf().size());
}

bool PkString::isEmpty() const
{
    return _cbuf().empty();
}

char16_t PkString::at(int i) const
{
    const std::vector<char16_t>& b = _cbuf();
    if (i < 0 || static_cast<std::size_t>(i) >= b.size()) {
        return u'\0';
    }
    return b[static_cast<std::size_t>(i)];
}

char16_t PkString::operator[](int i) const
{
    return at(i);
}

bool PkString::operator==(const PkString& other) const
{
    return _cbuf() == other._cbuf();
}

bool PkString::operator!=(const PkString& other) const
{
    return !(*this == other);
}

bool PkString::operator<(const PkString& other) const
{
    return _cbuf() < other._cbuf();
}

PkString PkString::operator+(const PkString& other) const
{
    PkString r(*this);
    r += other;
    return r;
}

PkString& PkString::operator+=(const PkString& other)
{
    if (other.isEmpty()) {
        return *this;
    }
    if (_d.PkIsSharedWith(other._d)) {
        // 自我追加：先把源拷出来，否则 detach/insert 会在自己的缓冲区上迭代
        const std::vector<char16_t> src = other._cbuf();
        std::vector<char16_t>& buf = _data();
        buf.insert(buf.end(), src.begin(), src.end());
    } else {
        const std::vector<char16_t>& src = other._cbuf();
        std::vector<char16_t>& buf = _data();
        buf.insert(buf.end(), src.begin(), src.end());
    }
    return *this;
}

std::u16string PkString::PkToU16() const
{
    const std::vector<char16_t>& b = _cbuf();
    return std::u16string(b.begin(), b.end());
}

std::string PkString::PkToUtf8() const
{
    return PkStringCodec::ToUtf8(_cbuf());
}

PkString PkString::PkFromUtf8(const char* s, int len)
{
    PkString r;
    const std::size_t n = (len < 0) ? (s != nullptr ? std::strlen(s) : 0)
                                    : static_cast<std::size_t>(len);
    r._data() = PkStringCodec::FromUtf8(s, n);
    return r;
}
