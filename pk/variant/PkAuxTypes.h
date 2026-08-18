#pragma once

#include <cstdint>
#include <vector>

#include "PkPoint.h"
#include "PkSize.h"

// ---------------------------------------------------------------------------
// PkByteArray —— QByteArray 的零 Qt 替代（最小实现）。
// 仅提供 PkVariant 所需的构造/查询/比较。
// ---------------------------------------------------------------------------
class PkByteArray
{
public:
    PkByteArray();
    PkByteArray(const char* data, int len);
    explicit PkByteArray(const std::vector<uint8_t>& data);

    const uint8_t* data() const;
    int size() const;
    bool isEmpty() const;
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