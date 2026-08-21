/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "PkTextStream.h"

#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

PkTextStream::PkTextStream(PkStream *device)
    : m_device(device)
    , m_pos(0)
{
    // 构造时把 device 剩余内容读进内部读缓冲。PkStream::readAll()（PkByteArray
    // 形态）未定义（pk/port 刻意声明不定义），这里用 char* read() 循环读全。
    if (!m_device) {
        return;
    }
    std::vector<uint8_t> buf;
    char chunk[8192];
    PkStream::pk_int64 n = 0;
    while ((n = m_device->read(chunk, static_cast<PkStream::pk_int64>(sizeof(chunk)))) > 0) {
        buf.insert(buf.end(), chunk, chunk + n);
    }
    m_readBuf = PkByteArray(buf);
}

PkTextStream::~PkTextStream()
{
    flush();
}

void PkTextStream::appendToWriteBuf(const char *data, std::size_t len)
{
    const int old = m_writeBuf.size();
    m_writeBuf.resize(old + static_cast<int>(len));
    std::memcpy(m_writeBuf.data() + old, data, len);
}

bool PkTextStream::atEnd() const
{
    return m_pos >= static_cast<std::size_t>(m_readBuf.size());
}

PkString PkTextStream::readAll()
{
    if (atEnd()) {
        return PkString();
    }
    const std::size_t remain = static_cast<std::size_t>(m_readBuf.size()) - m_pos;
    PkString result = PkString::PkFromUtf8(m_readBuf.data() + m_pos, static_cast<int>(remain));
    m_pos = static_cast<std::size_t>(m_readBuf.size());
    return result;
}

PkString PkTextStream::readLine()
{
    if (atEnd()) {
        return PkString();
    }
    const char *data = m_readBuf.data();
    const std::size_t size = static_cast<std::size_t>(m_readBuf.size());

    std::size_t i = m_pos;
    while (i < size && data[i] != '\n') {
        ++i;
    }
    std::size_t lineLen = i - m_pos;
    // CRLF：去掉行尾 '\r'（Qt readLine 对 CRLF 把 '\r' 一并吃掉）。
    if (lineLen > 0 && data[m_pos + lineLen - 1] == '\r') {
        --lineLen;
    }
    PkString line = PkString::PkFromUtf8(data + m_pos, static_cast<int>(lineLen));
    m_pos = (i < size) ? (i + 1) : i;   // 越过 '\n'
    return line;
}

bool PkTextStream::readLineInto(PkString &line)
{
    if (atEnd()) {
        line = PkString();
        return false;
    }
    line = readLine();
    return true;
}

bool PkTextStream::skipWhitespace()
{
    const char *data = m_readBuf.data();
    const std::size_t size = static_cast<std::size_t>(m_readBuf.size());
    while (m_pos < size) {
        const char c = data[m_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
            ++m_pos;
        } else {
            break;
        }
    }
    return m_pos < size;
}

bool PkTextStream::readToken(PkString &out)
{
    const char *data = m_readBuf.data();
    const std::size_t size = static_cast<std::size_t>(m_readBuf.size());
    const std::size_t start = m_pos;
    while (m_pos < size) {
        const char c = data[m_pos];
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v') {
            break;
        }
        ++m_pos;
    }
    if (m_pos == start) {
        out = PkString();
        return false;
    }
    out = PkString::PkFromUtf8(data + start, static_cast<int>(m_pos - start));
    return true;
}

PkTextStream &PkTextStream::operator>>(int &v)
{
    v = 0;
    if (!skipWhitespace()) {
        return *this;
    }
    const char *data = m_readBuf.data();
    const std::size_t size = static_cast<std::size_t>(m_readBuf.size());

    bool neg = false;
    std::size_t i = m_pos;
    if (i < size && (data[i] == '-' || data[i] == '+')) {
        neg = (data[i] == '-');
        ++i;
    }
    long long acc = 0;
    bool any = false;
    while (i < size && data[i] >= '0' && data[i] <= '9') {
        acc = acc * 10 + (data[i] - '0');
        any = true;
        ++i;
    }
    if (!any) {
        return *this;   // 非数字：不推进游标
    }
    m_pos = i;
    v = static_cast<int>(neg ? -acc : acc);
    return *this;
}

PkTextStream &PkTextStream::operator>>(float &v)
{
    v = 0.0f;
    if (!skipWhitespace()) {
        return *this;
    }
    PkString token;
    if (!readToken(token)) {
        return *this;
    }
    const std::string utf8 = token.PkToUtf8();
    v = static_cast<float>(std::strtod(utf8.c_str(), nullptr));
    return *this;
}

PkTextStream &PkTextStream::operator>>(PkString &v)
{
    v = PkString();
    if (!skipWhitespace()) {
        return *this;
    }
    readToken(v);
    return *this;
}

PkTextStream &PkTextStream::operator<<(const PkString &s)
{
    const std::string utf8 = s.PkToUtf8();
    appendToWriteBuf(utf8.data(), utf8.size());
    return *this;
}

PkTextStream &PkTextStream::operator<<(const char *s)
{
    if (s) {
        appendToWriteBuf(s, std::char_traits<char>::length(s));
    }
    return *this;
}

PkTextStream &PkTextStream::operator<<(int v)
{
    const std::string s = std::to_string(v);
    appendToWriteBuf(s.data(), s.size());
    return *this;
}

PkTextStream &PkTextStream::operator<<(char c)
{
    appendToWriteBuf(&c, 1);
    return *this;
}

void PkTextStream::flush()
{
    if (m_writeBuf.size() > 0 && m_device) {
        const char *data = m_writeBuf.data();
        std::size_t remaining = static_cast<std::size_t>(m_writeBuf.size());
        std::size_t written = 0;
        while (written < remaining) {
            const PkStream::pk_int64 n = m_device->write(
                data + written, static_cast<PkStream::pk_int64>(remaining - written));
            if (n <= 0) {
                break;
            }
            written += static_cast<std::size_t>(n);
        }
        m_writeBuf.resize(0);
    }
}

void PkTextStream::setCodec(const char * /*codecName*/)
{
    // 内部统一 UTF-8，无需转换。
}

void PkTextStream::setAutoDetectUnicode(bool /*enabled*/)
{
    // no-op：本垫片总是按 UTF-8 读。
}
