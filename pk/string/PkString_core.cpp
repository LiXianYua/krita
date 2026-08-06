#include "PkString.h"

#include "PkStringData.h"

#include <cstring>
#include <utility>

namespace {

// 移动走之后 _d 为空，所有只读路径统一落到这个空缓冲区上。
const std::vector<char16_t>& pkEmptyBuf()
{
    static const std::vector<char16_t> kEmpty;
    return kEmpty;
}

} // namespace

PkString::PkString()
    : _d(PkStringData::PkMakeEmpty())
{
}

PkString::PkString(const char* utf8)
    : _d(PkStringData::PkFromUtf8(utf8, utf8 != nullptr ? std::strlen(utf8) : 0))
{
}

PkString::PkString(const PkString& other)
    : _d(other._d)
{
}

PkString::PkString(PkString&& other) noexcept
    : _d(std::move(other._d))
{
}

PkString::~PkString() = default;

PkString& PkString::operator=(const PkString& other)
{
    _d = other._d;
    return *this;
}

PkString& PkString::operator=(PkString&& other) noexcept
{
    if (this != &other) {
        _d = std::move(other._d);
    }
    return *this;
}

const std::vector<char16_t>& PkString::_cbuf() const
{
    return _d ? _d->buf : pkEmptyBuf();
}

// COW 的全部复杂度就这一行：独占就原地改，共享就先分裂。
// 不做内存池、不做短串优化——性能问题按 W5 要用 M0 基线实测再说。
void PkString::_detach()
{
    if (!_d) {
        _d = PkStringData::PkMakeEmpty();
    } else if (_d.use_count() > 1) {
        _d = PkStringData::PkClone(*_d);
    }
}

const char16_t* PkString::_cdata() const
{
    return _cbuf().data();
}

char16_t* PkString::_data()
{
    _detach();
    return _d->buf.data();
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
    if (_d && _d == other._d) {
        // 自我追加：先把源拷出来，否则 detach/insert 会在自己的缓冲区上迭代
        const std::vector<char16_t> src = other._cbuf();
        _detach();
        _d->buf.insert(_d->buf.end(), src.begin(), src.end());
    } else {
        const std::vector<char16_t>& src = other._cbuf();
        _detach();
        _d->buf.insert(_d->buf.end(), src.begin(), src.end());
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
    return PkStringData::PkToUtf8(_cbuf());
}

PkString PkString::PkFromUtf8(const char* s, int len)
{
    PkString r;
    const std::size_t n = (len < 0) ? (s != nullptr ? std::strlen(s) : 0)
                                    : static_cast<std::size_t>(len);
    r._d = PkStringData::PkFromUtf8(s, n);
    return r;
}
