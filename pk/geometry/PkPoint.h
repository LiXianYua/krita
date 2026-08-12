#ifndef PK_GEOMETRY_PKPOINT_H
#define PK_GEOMETRY_PKPOINT_H

#include "PkGlobal.h"

// ---------------------------------------------------------------------------
// PkPoint / PkPointF —— QPoint / QPointF 的零 Qt 替代。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtCore/qpoint.h（QT_VERSION_STR "5.15.7"），
// 来源行号标在各项上方。对齐口径：与 Qt 的任何行为差异默认都是缺陷，所以 Qt
// 那些看着像 bug 的地方也照抄，并在 tests/test_point.cpp 与 oracle/ 里钉住：
//   · QPoint * qreal / QPoint / qreal 走 qRound，而 qRound 对负半值向 +∞ 取整
//     → QPoint(-1,-1)*0.5 == (0,0)、QPoint(-3,-3)*0.5 == (-1,-1)（实测真 Qt）
//   · float 与 double 两个重载**不是**摆设：QPoint(1,1)*0.49999997f == (1,1)
//     而同一数值走 double 得 (0,0)（float 精度下 +0.5 进位）
//   · QPointF::operator== 是**模糊比较**，不是位相等，且 inf==inf 为 false
//     而 inf==-inf 为 true（qFuzzyCompare 的公式在 ±inf 上的退化，实测确认）
//   · QPointF::isNull() 用 qIsNull（即 d == 0.0），所以 (-0.0,-0.0) 也是 null
//
// Qt 宏到 C++17 的映射（无行为差异，README 偏离清单里登记）：
//   Q_DECL_CONSTEXPR          → constexpr
//   Q_DECL_RELAXED_CONSTEXPR  → constexpr（C++14 起放宽的 constexpr，C++17 恒成立）
//   Q_CORE_EXPORT / Q_DECLARE_TYPEINFO / QT_WARNING_* → 去掉（可见性、容器移动
//   优化提示、-Wfloat-equal 的诊断压制，都不进入可观察行为）
//
// 明确不实现（族内实测用量 0）：dotProduct / fromCGPoint / toCGPoint，
// 以及 qHash、QDataStream 的 <<>>、QDebug 的 <<（归 R-02 / R-12 / R-08）。
// 详见 README.md 的「覆盖度缺口」。
// ---------------------------------------------------------------------------

class PkPoint
{
public:
    constexpr PkPoint();
    constexpr PkPoint(int xpos, int ypos);

    constexpr inline bool isNull() const;

    constexpr inline int x() const;
    constexpr inline int y() const;
    constexpr inline void setX(int x);
    constexpr inline void setY(int y);

    constexpr inline int manhattanLength() const;

    // qpoint.h:67
    constexpr PkPoint transposed() const noexcept { return {yp, xp}; }

    constexpr inline int &rx();
    constexpr inline int &ry();

    constexpr inline PkPoint &operator+=(const PkPoint &p);
    constexpr inline PkPoint &operator-=(const PkPoint &p);

    // qpoint.h:75-77 —— 三个重载，float 与 double **不能合并**（见文件头）。
    constexpr inline PkPoint &operator*=(float factor);
    constexpr inline PkPoint &operator*=(double factor);
    constexpr inline PkPoint &operator*=(int factor);

    constexpr inline PkPoint &operator/=(qreal divisor);

    // qpoint.h:81-82。**用量表说这个是 0 次，实测是错的** —— 那份导出只数
    // `.name(` 与 `->name(`，静态调用 `QPointF::dotProduct(...)` 落在别处。
    // 真实调用点：plugins/tools/basictools/kis_tool_measure.cc:139
    //（`.cc` 文件，只数 `.cpp` 的口径还会再漏一次）。族并集规则 → 两个类型都做。
    // 不防溢出，照抄：实测 dotProduct((INT_MAX,0),(2,0)) == -2。
    constexpr static inline int dotProduct(const PkPoint &p1, const PkPoint &p2)
    { return p1.xp * p2.xp + p1.yp * p2.yp; }

    friend constexpr inline bool operator==(const PkPoint &, const PkPoint &);
    friend constexpr inline bool operator!=(const PkPoint &, const PkPoint &);
    friend constexpr inline const PkPoint operator+(const PkPoint &, const PkPoint &);
    friend constexpr inline const PkPoint operator-(const PkPoint &, const PkPoint &);
    friend constexpr inline const PkPoint operator*(const PkPoint &, float);
    friend constexpr inline const PkPoint operator*(float, const PkPoint &);
    friend constexpr inline const PkPoint operator*(const PkPoint &, double);
    friend constexpr inline const PkPoint operator*(double, const PkPoint &);
    friend constexpr inline const PkPoint operator*(const PkPoint &, int);
    friend constexpr inline const PkPoint operator*(int, const PkPoint &);
    friend constexpr inline const PkPoint operator+(const PkPoint &);
    friend constexpr inline const PkPoint operator-(const PkPoint &);
    friend constexpr inline const PkPoint operator/(const PkPoint &, qreal);

private:
    // qpoint.h:103 —— QTransform 直接读写 xp/yp。Task 6 交付 PkTransform 时要用。
    friend class PkTransform;
    int xp;
    int yp;
};

// ── PkPoint inline（qpoint.h:122-211）──────────────────────────────────────

constexpr inline PkPoint::PkPoint() : xp(0), yp(0) {}

constexpr inline PkPoint::PkPoint(int xpos, int ypos) : xp(xpos), yp(ypos) {}

constexpr inline bool PkPoint::isNull() const
{ return xp == 0 && yp == 0; }

constexpr inline int PkPoint::x() const
{ return xp; }

constexpr inline int PkPoint::y() const
{ return yp; }

constexpr inline void PkPoint::setX(int xpos)
{ xp = xpos; }

constexpr inline void PkPoint::setY(int ypos)
{ yp = ypos; }

// qpoint.h:141-142。**不防溢出**：QPoint(INT_MIN,0).manhattanLength() 实测得
// INT_MIN（qAbs(INT_MIN) 回绕），QPoint(INT_MAX,INT_MAX) 得 -2。照抄。
constexpr inline int PkPoint::manhattanLength() const
{ return qAbs(x())+qAbs(y()); }

constexpr inline int &PkPoint::rx()
{ return xp; }

constexpr inline int &PkPoint::ry()
{ return yp; }

constexpr inline PkPoint &PkPoint::operator+=(const PkPoint &p)
{ xp+=p.xp; yp+=p.yp; return *this; }

constexpr inline PkPoint &PkPoint::operator-=(const PkPoint &p)
{ xp-=p.xp; yp-=p.yp; return *this; }

constexpr inline PkPoint &PkPoint::operator*=(float factor)
{ xp = qRound(xp*factor); yp = qRound(yp*factor); return *this; }

constexpr inline PkPoint &PkPoint::operator*=(double factor)
{ xp = qRound(xp*factor); yp = qRound(yp*factor); return *this; }

constexpr inline PkPoint &PkPoint::operator*=(int factor)
{ xp = xp*factor; yp = yp*factor; return *this; }

constexpr inline bool operator==(const PkPoint &p1, const PkPoint &p2)
{ return p1.xp == p2.xp && p1.yp == p2.yp; }

constexpr inline bool operator!=(const PkPoint &p1, const PkPoint &p2)
{ return p1.xp != p2.xp || p1.yp != p2.yp; }

constexpr inline const PkPoint operator+(const PkPoint &p1, const PkPoint &p2)
{ return PkPoint(p1.xp+p2.xp, p1.yp+p2.yp); }

constexpr inline const PkPoint operator-(const PkPoint &p1, const PkPoint &p2)
{ return PkPoint(p1.xp-p2.xp, p1.yp-p2.yp); }

constexpr inline const PkPoint operator*(const PkPoint &p, float factor)
{ return PkPoint(qRound(p.xp*factor), qRound(p.yp*factor)); }

constexpr inline const PkPoint operator*(const PkPoint &p, double factor)
{ return PkPoint(qRound(p.xp*factor), qRound(p.yp*factor)); }

constexpr inline const PkPoint operator*(const PkPoint &p, int factor)
{ return PkPoint(p.xp*factor, p.yp*factor); }

constexpr inline const PkPoint operator*(float factor, const PkPoint &p)
{ return PkPoint(qRound(p.xp*factor), qRound(p.yp*factor)); }

constexpr inline const PkPoint operator*(double factor, const PkPoint &p)
{ return PkPoint(qRound(p.xp*factor), qRound(p.yp*factor)); }

constexpr inline const PkPoint operator*(int factor, const PkPoint &p)
{ return PkPoint(p.xp*factor, p.yp*factor); }

// qpoint.h:195-196 —— 一元 + 返回**原样**（含零号的符号位），不是 qAbs。
constexpr inline const PkPoint operator+(const PkPoint &p)
{ return p; }

constexpr inline const PkPoint operator-(const PkPoint &p)
{ return PkPoint(-p.xp, -p.yp); }

constexpr inline PkPoint &PkPoint::operator/=(qreal c)
{
    xp = qRound(xp/c);
    yp = qRound(yp/c);
    return *this;
}

constexpr inline const PkPoint operator/(const PkPoint &p, qreal c)
{
    return PkPoint(qRound(p.xp/c), qRound(p.yp/c));
}


class PkPointF
{
public:
    constexpr PkPointF();
    // qpoint.h:225 —— **非 explicit**：PkPoint 到 PkPointF 是隐式提升，
    // Krita 里大量调用点靠这条（`somePointF + somePoint` 能编过）。
    constexpr PkPointF(const PkPoint &p);
    constexpr PkPointF(qreal xpos, qreal ypos);

    constexpr inline qreal manhattanLength() const;

    // qpoint.h:230 —— Qt 这一个**不是** constexpr（qIsNull 在 5.15 才变 constexpr，
    // 声明没跟着改）。照抄，免得 constexpr 上下文里两边可用性不同。
    inline bool isNull() const;

    constexpr inline qreal x() const;
    constexpr inline qreal y() const;
    constexpr inline void setX(qreal x);
    constexpr inline void setY(qreal y);

    constexpr PkPointF transposed() const noexcept { return {yp, xp}; }

    constexpr inline qreal &rx();
    constexpr inline qreal &ry();

    constexpr inline PkPointF &operator+=(const PkPointF &p);
    constexpr inline PkPointF &operator-=(const PkPointF &p);
    constexpr inline PkPointF &operator*=(qreal c);
    constexpr inline PkPointF &operator/=(qreal c);

    // qpoint.h:247-248 —— 真实调用点在 kis_tool_measure.cc:139，见 PkPoint 那条。
    constexpr static inline qreal dotProduct(const PkPointF &p1, const PkPointF &p2)
    { return p1.xp * p2.xp + p1.yp * p2.yp; }

    friend constexpr inline bool operator==(const PkPointF &, const PkPointF &);
    friend constexpr inline bool operator!=(const PkPointF &, const PkPointF &);
    friend constexpr inline const PkPointF operator+(const PkPointF &, const PkPointF &);
    friend constexpr inline const PkPointF operator-(const PkPointF &, const PkPointF &);
    friend constexpr inline const PkPointF operator*(qreal, const PkPointF &);
    friend constexpr inline const PkPointF operator*(const PkPointF &, qreal);
    friend constexpr inline const PkPointF operator+(const PkPointF &);
    friend constexpr inline const PkPointF operator-(const PkPointF &);
    friend constexpr inline const PkPointF operator/(const PkPointF &, qreal);

    constexpr PkPoint toPoint() const;

private:
    friend class PkTransform;

    qreal xp;
    qreal yp;
};

// ── PkPointF inline（qpoint.h:289-415）─────────────────────────────────────

constexpr inline PkPointF::PkPointF() : xp(0), yp(0) { }

constexpr inline PkPointF::PkPointF(qreal xpos, qreal ypos) : xp(xpos), yp(ypos) { }

constexpr inline PkPointF::PkPointF(const PkPoint &p) : xp(p.x()), yp(p.y()) { }

constexpr inline qreal PkPointF::manhattanLength() const
{
    return qAbs(x())+qAbs(y());
}

// qpoint.h:300-303 用的是 qIsNull(xp) && qIsNull(yp)，而 qglobal.h:925-928 的
// qIsNull(double d) 就是 `d == 0.0` —— 于是 **-0.0 也算 null**（实测真 Qt：
// QPointF(-0.0,-0.0).isNull() == true），而 5e-324 不算。
// 这里直接写出那个比较，不把 qIsNull 这个名字提进 compat：它在 Krita 保留范围
// 内实测 0 调用点，导出去就违反判据①「一项不多」。
inline bool PkPointF::isNull() const
{
    return xp == 0.0 && yp == 0.0;
}

constexpr inline qreal PkPointF::x() const
{
    return xp;
}

constexpr inline qreal PkPointF::y() const
{
    return yp;
}

constexpr inline void PkPointF::setX(qreal xpos)
{
    xp = xpos;
}

constexpr inline void PkPointF::setY(qreal ypos)
{
    yp = ypos;
}

constexpr inline qreal &PkPointF::rx()
{
    return xp;
}

constexpr inline qreal &PkPointF::ry()
{
    return yp;
}

constexpr inline PkPointF &PkPointF::operator+=(const PkPointF &p)
{
    xp+=p.xp;
    yp+=p.yp;
    return *this;
}

constexpr inline PkPointF &PkPointF::operator-=(const PkPointF &p)
{
    xp-=p.xp; yp-=p.yp; return *this;
}

constexpr inline PkPointF &PkPointF::operator*=(qreal c)
{
    xp*=c; yp*=c; return *this;
}

// qpoint.h:357-361。**模糊比较，不是位相等。** 每个分量二选一：只要有一侧
// 恰好是 0（含 -0.0），比的是差值 fuzzyIsNull（绝对阈值 1e-12）；否则比相对
// 误差 fuzzyCompare（相对阈值 1e-12）。由此产生两个反直觉的实测事实：
//   QPointF(inf,0)  == QPointF(inf,0)  → **false**（|inf-inf| = nan，nan<=x 恒假）
//   QPointF(inf,0)  == QPointF(-inf,0) → **true** （|inf-(-inf)| = inf <= inf）
// 走 pkQtFuzzy* 而不是 qFuzzy*：后者在共存路径上是 #define，会被改写成
// pk/test 的实现（理由见 PkGlobal.h 那段注释）。公式与 Qt 逐字相同。
// Qt 在这里 QT_WARNING_DISABLE_*("-Wfloat-equal")；那只是诊断压制，不抄。
constexpr inline bool operator==(const PkPointF &p1, const PkPointF &p2)
{
    return ((!p1.xp || !p2.xp) ? pkQtFuzzyIsNull(p1.xp - p2.xp) : pkQtFuzzyCompare(p1.xp, p2.xp))
        && ((!p1.yp || !p2.yp) ? pkQtFuzzyIsNull(p1.yp - p2.yp) : pkQtFuzzyCompare(p1.yp, p2.yp));
}

constexpr inline bool operator!=(const PkPointF &p1, const PkPointF &p2)
{
    return !(p1 == p2);
}

constexpr inline const PkPointF operator+(const PkPointF &p1, const PkPointF &p2)
{
    return PkPointF(p1.xp+p2.xp, p1.yp+p2.yp);
}

constexpr inline const PkPointF operator-(const PkPointF &p1, const PkPointF &p2)
{
    return PkPointF(p1.xp-p2.xp, p1.yp-p2.yp);
}

constexpr inline const PkPointF operator*(const PkPointF &p, qreal c)
{
    return PkPointF(p.xp*c, p.yp*c);
}

constexpr inline const PkPointF operator*(qreal c, const PkPointF &p)
{
    return PkPointF(p.xp*c, p.yp*c);
}

constexpr inline const PkPointF operator+(const PkPointF &p)
{
    return p;
}

constexpr inline const PkPointF operator-(const PkPointF &p)
{
    return PkPointF(-p.xp, -p.yp);
}

constexpr inline PkPointF &PkPointF::operator/=(qreal divisor)
{
    xp/=divisor;
    yp/=divisor;
    return *this;
}

constexpr inline const PkPointF operator/(const PkPointF &p, qreal divisor)
{
    return PkPointF(p.xp/divisor, p.yp/divisor);
}

// qpoint.h:412-415 —— qRound，**不是截断**。qRound 对负半值向 +∞ 取整，
// 所以 PkPointF(-0.5,-0.5).toPoint() == (0,0)（实测真 Qt 5.15.7）。
constexpr inline PkPoint PkPointF::toPoint() const
{
    return PkPoint(qRound(xp), qRound(yp));
}

#endif // PK_GEOMETRY_PKPOINT_H
