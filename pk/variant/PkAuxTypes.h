#pragma once

#include <cstdint>
#include <vector>

#include "PkPoint.h"
#include "PkSize.h"

// ---------------------------------------------------------------------------
// PkByteArray —— QByteArray 的零 Qt 替代。
// API 面与语义对齐 Qt 5.15（resize/number/data/constData 均由探针实测钉住，
// 见 pk/variant/oracle/ 下的对拍 ba_oracle）。
// ---------------------------------------------------------------------------
class PkByteArray
{
public:
    PkByteArray();
    PkByteArray(const char* data, int len);          // len<=0 按空处理（Qt 对 (char*,0) 合法）
    explicit PkByteArray(const std::vector<uint8_t>& data);

    // 对齐 QByteArray：data() 有可变/const 两个重载，constData() 恒 const。
    char*        data();                              // 可变
    const char*  data() const;                        // 空时返回非空 NUL 指针
    const char*  constData() const;                   // 空时返回非空 NUL 指针（探针）
    int          size() const;
    bool         isEmpty() const;

    // 对齐 QByteArray::resize（探针实测语义）：
    //   n<=0 → 清空（size 0）；n>size → 尾部补 0；n<size → 截断保留前缀。
    void         resize(int n);

    // 对齐 QByteArray::number（探针实测语义）：
    //   number(int, base)：base==10 带符号十进制；base==2/8/16 把 int 当 uint32
    //   打全 32 位补码，小写。
    //   number(uint, base)：base==10 无符号十进制；base==2/8/16 直接无符号，小写。
    static PkByteArray number(int n, int base = 10);
    static PkByteArray number(unsigned int n, int base = 10);

    bool operator==(const PkByteArray& other) const;
    bool operator!=(const PkByteArray& other) const;

private:
    std::vector<uint8_t> m_data;
};

// ---------------------------------------------------------------------------
// PkLine —— QLine 的零 Qt 替代。
// ---------------------------------------------------------------------------
class PkLine
{
public:
    PkLine();
    PkLine(const PkPoint& p1, const PkPoint& p2);
    PkLine(int x1, int y1, int x2, int y2);

    PkPoint p1() const;
    PkPoint p2() const;
    int x1() const;
    int y1() const;
    int x2() const;
    int y2() const;
    void setP1(const PkPoint& p);
    void setP2(const PkPoint& p);
    void setLine(int x1, int y1, int x2, int y2);

    bool isNull() const;
    bool operator==(const PkLine& other) const;
    bool operator!=(const PkLine& other) const;

private:
    PkPoint m_p1;
    PkPoint m_p2;
};

// ---------------------------------------------------------------------------
// PkLineF —— QLineF 的零 Qt 替代。
// ---------------------------------------------------------------------------
class PkLineF
{
public:
    PkLineF();
    PkLineF(const PkPointF& p1, const PkPointF& p2);
    PkLineF(qreal x1, qreal y1, qreal x2, qreal y2);

    PkPointF p1() const;
    PkPointF p2() const;
    qreal x1() const;
    qreal y1() const;
    qreal x2() const;
    qreal y2() const;
    void setP1(const PkPointF& p);
    void setP2(const PkPointF& p);
    void setLine(qreal x1, qreal y1, qreal x2, qreal y2);

    bool isNull() const;
    bool operator==(const PkLineF& other) const;
    bool operator!=(const PkLineF& other) const;

private:
    PkPointF m_p1;
    PkPointF m_p2;
};

// ---------------------------------------------------------------------------
// PkDate —— QDate 的零 Qt 替代。
// 默认构造为 invalid（与 QDate() 一致）。
// ---------------------------------------------------------------------------
class PkDate
{
public:
    PkDate();
    PkDate(int y, int m, int d);

    int year() const;
    int month() const;
    int day() const;
    bool isValid() const;
    bool isNull() const;
    bool operator==(const PkDate& other) const;
    bool operator!=(const PkDate& other) const;

private:
    int m_year;
    int m_month;
    int m_day;
};

// ---------------------------------------------------------------------------
// PkTime —— QTime 的零 Qt 替代。
// 默认构造为 invalid（与 QTime() 一致）。
// ---------------------------------------------------------------------------
class PkTime
{
public:
    PkTime();
    PkTime(int h, int m, int s = 0, int ms = 0);

    int hour() const;
    int minute() const;
    int second() const;
    int msec() const;
    bool isValid() const;
    bool isNull() const;
    bool operator==(const PkTime& other) const;
    bool operator!=(const PkTime& other) const;

private:
    int m_hour;
    int m_minute;
    int m_second;
    int m_msec;
};

// ---------------------------------------------------------------------------
// PkDateTime —— QDateTime 的零 Qt 替代。
// 默认构造为 invalid（与 QDateTime() 一致）。
// ---------------------------------------------------------------------------
class PkDateTime
{
public:
    PkDateTime();
    PkDateTime(const PkDate& date, const PkTime& time);

    PkDate date() const;
    PkTime time() const;
    bool isValid() const;
    bool isNull() const;
    bool operator==(const PkDateTime& other) const;
    bool operator!=(const PkDateTime& other) const;

private:
    PkDate m_date;
    PkTime m_time;
};