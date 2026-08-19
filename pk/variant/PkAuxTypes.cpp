#include "PkAuxTypes.h"

#include <string>

// ── PkByteArray ────────────────────────────────────────────────────────────

PkByteArray::PkByteArray() = default;

PkByteArray::PkByteArray(const char* data, int len)
{
    if (len <= 0) return;
    m_data.reserve(static_cast<size_t>(len));
    for (int i = 0; i < len; ++i)
        m_data.push_back(static_cast<uint8_t>(data[i]));
}

PkByteArray::PkByteArray(const std::vector<uint8_t>& data) : m_data(data) {}

char* PkByteArray::data() { return reinterpret_cast<char*>(m_data.data()); }

const char* PkByteArray::data() const
{
    return m_data.empty() ? "" : reinterpret_cast<const char*>(m_data.data());
}

const char* PkByteArray::constData() const
{
    return m_data.empty() ? "" : reinterpret_cast<const char*>(m_data.data());
}

int PkByteArray::size() const { return static_cast<int>(m_data.size()); }
bool PkByteArray::isEmpty() const { return m_data.empty(); }

void PkByteArray::resize(int n)
{
    if (n <= 0) { m_data.clear(); return; }
    m_data.resize(static_cast<size_t>(n));
}

PkByteArray PkByteArray::number(int n, int base)
{
    if (base == 10) {
        std::string s = std::to_string(n);
        return PkByteArray(s.data(), static_cast<int>(s.size()));
    }
    // base==2/8/16（及任意非 10 进制）：把 int 当 uint32 打全位补码。
    return number(static_cast<unsigned int>(n), base);
}

PkByteArray PkByteArray::number(unsigned int n, int base)
{
    if (base == 10) {
        std::string s = std::to_string(n);
        return PkByteArray(s.data(), static_cast<int>(s.size()));
    }
    if (base < 2) base = 2;
    if (base > 36) base = 36;
    static const char digits[] = "0123456789abcdefghijklmnopqrstuvwxyz";
    char buf[40];
    char* p = buf + sizeof(buf);
    unsigned int u = n;
    const unsigned int ub = static_cast<unsigned int>(base);
    do {
        *--p = digits[u % ub];
        u /= ub;
    } while (u > 0);
    return PkByteArray(p, static_cast<int>(buf + sizeof(buf) - p));
}

bool PkByteArray::operator==(const PkByteArray& other) const { return m_data == other.m_data; }
bool PkByteArray::operator!=(const PkByteArray& other) const { return !(*this == other); }

// ── PkLine ─────────────────────────────────────────────────────────────────

PkLine::PkLine() : m_p1(), m_p2() {}
PkLine::PkLine(const PkPoint& p1, const PkPoint& p2) : m_p1(p1), m_p2(p2) {}
PkLine::PkLine(int x1, int y1, int x2, int y2) : m_p1(x1, y1), m_p2(x2, y2) {}

PkPoint PkLine::p1() const { return m_p1; }
PkPoint PkLine::p2() const { return m_p2; }
int PkLine::x1() const { return m_p1.x(); }
int PkLine::y1() const { return m_p1.y(); }
int PkLine::x2() const { return m_p2.x(); }
int PkLine::y2() const { return m_p2.y(); }
void PkLine::setP1(const PkPoint& p) { m_p1 = p; }
void PkLine::setP2(const PkPoint& p) { m_p2 = p; }
void PkLine::setLine(int x1, int y1, int x2, int y2) { m_p1 = PkPoint(x1, y1); m_p2 = PkPoint(x2, y2); }

bool PkLine::isNull() const { return m_p1 == m_p2; }
bool PkLine::operator==(const PkLine& other) const { return m_p1 == other.m_p1 && m_p2 == other.m_p2; }
bool PkLine::operator!=(const PkLine& other) const { return !(*this == other); }

// ── PkLineF ────────────────────────────────────────────────────────────────

PkLineF::PkLineF() : m_p1(), m_p2() {}
PkLineF::PkLineF(const PkPointF& p1, const PkPointF& p2) : m_p1(p1), m_p2(p2) {}
PkLineF::PkLineF(qreal x1, qreal y1, qreal x2, qreal y2) : m_p1(x1, y1), m_p2(x2, y2) {}

PkPointF PkLineF::p1() const { return m_p1; }
PkPointF PkLineF::p2() const { return m_p2; }
qreal PkLineF::x1() const { return m_p1.x(); }
qreal PkLineF::y1() const { return m_p1.y(); }
qreal PkLineF::x2() const { return m_p2.x(); }
qreal PkLineF::y2() const { return m_p2.y(); }
void PkLineF::setP1(const PkPointF& p) { m_p1 = p; }
void PkLineF::setP2(const PkPointF& p) { m_p2 = p; }
void PkLineF::setLine(qreal x1, qreal y1, qreal x2, qreal y2) { m_p1 = PkPointF(x1, y1); m_p2 = PkPointF(x2, y2); }

bool PkLineF::isNull() const { return m_p1 == m_p2; }
bool PkLineF::operator==(const PkLineF& other) const { return m_p1 == other.m_p1 && m_p2 == other.m_p2; }
bool PkLineF::operator!=(const PkLineF& other) const { return !(*this == other); }

// ── PkDate ─────────────────────────────────────────────────────────────────

PkDate::PkDate() : m_year(0), m_month(0), m_day(0) {}
PkDate::PkDate(int y, int m, int d) : m_year(y), m_month(m), m_day(d) {}

int PkDate::year() const { return m_year; }
int PkDate::month() const { return m_month; }
int PkDate::day() const { return m_day; }
bool PkDate::isValid() const { return m_year != 0 && m_month >= 1 && m_month <= 12 && m_day >= 1 && m_day <= 31; }
bool PkDate::isNull() const { return m_year == 0 && m_month == 0 && m_day == 0; }
bool PkDate::operator==(const PkDate& other) const { return m_year == other.m_year && m_month == other.m_month && m_day == other.m_day; }
bool PkDate::operator!=(const PkDate& other) const { return !(*this == other); }

// ── PkTime ─────────────────────────────────────────────────────────────────

PkTime::PkTime() : m_hour(-1), m_minute(-1), m_second(-1), m_msec(-1) {}
PkTime::PkTime(int h, int m, int s, int ms) : m_hour(h), m_minute(m), m_second(s), m_msec(ms) {}

int PkTime::hour() const { return m_hour; }
int PkTime::minute() const { return m_minute; }
int PkTime::second() const { return m_second; }
int PkTime::msec() const { return m_msec; }
bool PkTime::isValid() const { return m_hour >= 0 && m_hour <= 23 && m_minute >= 0 && m_minute <= 59 && m_second >= 0 && m_second <= 59 && m_msec >= 0 && m_msec <= 999; }
bool PkTime::isNull() const { return m_hour == -1 && m_minute == -1 && m_second == -1 && m_msec == -1; }
bool PkTime::operator==(const PkTime& other) const { return m_hour == other.m_hour && m_minute == other.m_minute && m_second == other.m_second && m_msec == other.m_msec; }
bool PkTime::operator!=(const PkTime& other) const { return !(*this == other); }

// ── PkDateTime ─────────────────────────────────────────────────────────────

PkDateTime::PkDateTime() : m_date(), m_time() {}
PkDateTime::PkDateTime(const PkDate& date, const PkTime& time) : m_date(date), m_time(time) {}

PkDate PkDateTime::date() const { return m_date; }
PkTime PkDateTime::time() const { return m_time; }
bool PkDateTime::isValid() const { return m_date.isValid() && m_time.isValid(); }
bool PkDateTime::isNull() const { return m_date.isNull() && m_time.isNull(); }
bool PkDateTime::operator==(const PkDateTime& other) const { return m_date == other.m_date && m_time == other.m_time; }
bool PkDateTime::operator!=(const PkDateTime& other) const { return !(*this == other); }