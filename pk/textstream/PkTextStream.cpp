#include "PkTextStream.h"
#include "PkStream.h"   // pk/port：PkStream::read/write/readLine/atEnd
#include <cstring>
#include <sstream>

PkTextStream &PkTextStream::operator<<(const char *s) {
    if (m_dev) {
        m_dev->write(s, static_cast<PkStream::pk_int64>(std::strlen(s)));
    } else if (m_str) {
        *m_str += s;
    } else if (m_file) {
        fputs(s, m_file);
    }
    return *this;
}

PkTextStream &PkTextStream::operator<<(const std::string &s) { return *this << s.c_str(); }
PkTextStream &PkTextStream::operator<<(char c) { char b[2] = {c, 0}; return *this << b; }
PkTextStream &PkTextStream::operator<<(int v) { return *this << std::to_string(v); }
PkTextStream &PkTextStream::operator<<(long v) { return *this << std::to_string(v); }
PkTextStream &PkTextStream::operator<<(unsigned v) { return *this << std::to_string(v); }
PkTextStream &PkTextStream::operator<<(double v) { return *this << formatReal(v); }
PkTextStream &PkTextStream::operator<<(float v) { return *this << formatReal(double(v)); }

PkTextStream &PkTextStream::operator>>(std::string &s) { s = readLine(); return *this; }
PkTextStream &PkTextStream::operator>>(int &v) { std::string t = readLine(); v = std::stoi(t); return *this; }
PkTextStream &PkTextStream::operator>>(double &v) { std::string t = readLine(); v = std::stod(t); return *this; }

std::string PkTextStream::readLine() {
    if (m_dev) {
        char buf[4096];
        PkStream::pk_int64 n = m_dev->readLine(buf, static_cast<PkStream::pk_int64>(sizeof(buf)));
        if (n < 0) return std::string();
        return std::string(buf, static_cast<std::size_t>(n));
    }
    if (m_str) {
        std::string &s = *m_str;
        std::size_t nl = s.find('\n');
        if (nl == std::string::npos) { std::string r = s; s.clear(); return r; }
        std::string r = s.substr(0, nl);   // 去掉行尾 '\n'（对齐 QTextStream::readLine）
        s.erase(0, nl + 1);
        return r;
    }
    if (m_file) {
        std::string r; char c;
        while ((c = static_cast<char>(fgetc(m_file))) != EOF && c != '\n') r += c;
        return r;
    }
    return std::string();
}

std::string PkTextStream::readAll() {
    if (m_dev) {
        std::string r; char buf[4096]; PkStream::pk_int64 n;
        while ((n = m_dev->read(buf, static_cast<PkStream::pk_int64>(sizeof(buf)))) > 0)
            r.append(buf, static_cast<std::size_t>(n));
        return r;
    }
    if (m_str) { std::string r = *m_str; m_str->clear(); return r; }
    if (m_file) {
        std::string r; char c;
        while ((c = static_cast<char>(fgetc(m_file))) != EOF) r += c;
        return r;
    }
    return std::string();
}

bool PkTextStream::atEnd() const {
    if (m_dev) return m_dev->atEnd();
    return true;
}

void PkTextStream::flush() { if (m_file) fflush(m_file); }

std::string PkTextStream::formatReal(double v) {
    std::ostringstream os; os.precision(m_prec > 0 ? m_prec : 6); os << v; return os.str();
}
