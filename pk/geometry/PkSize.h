#ifndef PK_GEOMETRY_PKSIZE_H
#define PK_GEOMETRY_PKSIZE_H

#include "PkGlobal.h"

// ---------------------------------------------------------------------------
// PkSize / PkSizeF —— QSize / QSizeF 的零 Qt 替代。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtCore/qsize.h（QT_VERSION_STR "5.15.7"）
// 与 qtbase v5.15.7-lts-lgpl 的 src/corelib/tools/qsize.cpp（两个 scaled()
// 是 out-of-line 的，装的 Qt 只有 .so 没有源，源码取自上游同一 tag 并**逐条用
// 探针复核过取值**）。来源行号标在各项上方。对齐口径：与 Qt 的任何行为差异默认
// 都是缺陷，所以 Qt 那些反直觉的地方也照抄，并在 tests/test_size.cpp 与 oracle/
// 里钉住：
//   · **默认构造是 (-1,-1)**，不是 (0,0)（QSizeF 也是 (-1.,-1.)）
//   · isNull / isEmpty / isValid **三条公式互不相同**，且
//     **QSize(0,0).isValid() == true** —— 与 QRect(0,0,0,0).isValid()==false 相反
//   · QSize::isEmpty 是 `wd<1||ht<1`，QSizeF::isEmpty 是 `wd<=0.||ht<=0.`
//     —— 整数上等价、浮点上不等价（(0.5,0.5) 在整数公式下会被误判成空）
//   · **源尺寸任一分量为 0 时 scaled 直接返回目标尺寸**，不做比例运算
//   · QSize::scaled 的中间量是 qint64，回窄到 int 时按二补数回绕
//   · QSizeF::operator== 是 qFuzzyCompare **逐分量各来一次，没有零分支**
//     —— 与 QPointF::operator== 的两分支写法不同：
//       QPointF(0,0)==QPointF(1e-300,0) 为 true，而 QSizeF 上同样的值是 false
//
// Qt 宏到 C++17 的映射（无行为差异，README 偏离清单里登记）：
//   Q_DECL_CONSTEXPR / Q_DECL_RELAXED_CONSTEXPR → constexpr
//   Q_CORE_EXPORT / Q_DECLARE_TYPEINFO / Q_REQUIRED_RESULT → 去掉
//   **noexcept 照抄**（它是可观察的：noexcept 运算符、容器移动选择），
//   包括「带 Q_ASSERT 的两个除法 Qt 恰好没标 noexcept」这个不对称。
//
// 明确不实现（族内实测用量 0，判据①「一项不多」；完整归属表在 README）：
//   · boundedTo / grownBy / shrunkBy / toCGSize / fromCGSize —— 三形态实测 0 次
//   · transpose / transposed —— 三形态命中的 5+5 处**接收者全不是 QSize 族**
//     （`transposed` 全是 QTransform、`transpose` 全是 Eigen 矩阵）
//   · Q_ASSERT(!qFuzzyIsNull(c))（两个除法里）—— 断言设施归 R-08，
//     且 Krita 发布构建把它整条编译掉（见 README 偏离清单）
//   · qHash / QDataStream 的 <<>> / QDebug 的 <<（归 R-02 / R-12 / R-08）
// ---------------------------------------------------------------------------

class PkSize
{
public:
    // qsize.h:123 —— ⚠ **(-1,-1)**，不是 (0,0)。
    constexpr PkSize() noexcept;
    constexpr PkSize(int w, int h) noexcept;

    constexpr inline bool isNull() const noexcept;
    constexpr inline bool isEmpty() const noexcept;
    constexpr inline bool isValid() const noexcept;

    constexpr inline int width() const noexcept;
    constexpr inline int height() const noexcept;
    constexpr inline void setWidth(int w) noexcept;
    constexpr inline void setHeight(int h) noexcept;

    inline void scale(int w, int h, Qt::AspectRatioMode mode) noexcept;
    inline void scale(const PkSize &s, Qt::AspectRatioMode mode) noexcept;
    PkSize scaled(int w, int h, Qt::AspectRatioMode mode) const noexcept;
    PkSize scaled(const PkSize &s, Qt::AspectRatioMode mode) const noexcept;

    constexpr inline PkSize expandedTo(const PkSize &) const noexcept;

    constexpr inline int &rwidth() noexcept;
    constexpr inline int &rheight() noexcept;

    constexpr inline PkSize &operator+=(const PkSize &) noexcept;
    constexpr inline PkSize &operator-=(const PkSize &) noexcept;
    constexpr inline PkSize &operator*=(qreal c) noexcept;
    // qsize.h:89 —— **没有 noexcept**（Qt 里这一个带 Q_ASSERT）。照抄。
    inline PkSize &operator/=(qreal c);

    friend inline constexpr bool operator==(const PkSize &, const PkSize &) noexcept;
    friend inline constexpr bool operator!=(const PkSize &, const PkSize &) noexcept;
    friend inline constexpr const PkSize operator+(const PkSize &, const PkSize &) noexcept;
    friend inline constexpr const PkSize operator-(const PkSize &, const PkSize &) noexcept;
    friend inline constexpr const PkSize operator*(const PkSize &, qreal) noexcept;
    friend inline constexpr const PkSize operator*(qreal, const PkSize &) noexcept;
    friend inline const PkSize operator/(const PkSize &, qreal);

private:
    // qsize.h:104-105。Task 4/5 的 PkRect 按公开 API 构造 PkSize（qrect.h 也是
    // `return QSize(width(), height());`），所以这里不需要 friend 声明。
    int wd;
    int ht;
};

// ── PkSize inline（qsize.h:123-214）───────────────────────────────────────

constexpr inline PkSize::PkSize() noexcept : wd(-1), ht(-1) {}

constexpr inline PkSize::PkSize(int w, int h) noexcept : wd(w), ht(h) {}

// qsize.h:127-134 —— 三条公式**逐字**照抄，一个符号都不能动：
//   isNull  只认 (0,0)；isEmpty 的门槛是 `<1`；isValid 允许 0。
// 于是 (0,0) 同时是 null、empty、**valid**，(10,0) 是 empty 且 valid，
// (-1,-1) 是 empty 且**不** valid。三者互不蕴含。
constexpr inline bool PkSize::isNull() const noexcept
{ return wd==0 && ht==0; }

constexpr inline bool PkSize::isEmpty() const noexcept
{ return wd<1 || ht<1; }

constexpr inline bool PkSize::isValid() const noexcept
{ return wd>=0 && ht>=0; }

constexpr inline int PkSize::width() const noexcept
{ return wd; }

constexpr inline int PkSize::height() const noexcept
{ return ht; }

constexpr inline void PkSize::setWidth(int w) noexcept
{ wd = w; }

constexpr inline void PkSize::setHeight(int h) noexcept
{ ht = h; }

// qsize.h:151-158 —— scale 就是 `*this = scaled(...)`，两个重载都只是转发。
inline void PkSize::scale(int w, int h, Qt::AspectRatioMode mode) noexcept
{ scale(PkSize(w, h), mode); }

inline void PkSize::scale(const PkSize &s, Qt::AspectRatioMode mode) noexcept
{ *this = scaled(s, mode); }

inline PkSize PkSize::scaled(int w, int h, Qt::AspectRatioMode mode) const noexcept
{ return scaled(PkSize(w, h), mode); }

constexpr inline int &PkSize::rwidth() noexcept
{ return wd; }

constexpr inline int &PkSize::rheight() noexcept
{ return ht; }

constexpr inline PkSize &PkSize::operator+=(const PkSize &s) noexcept
{ wd+=s.wd; ht+=s.ht; return *this; }

constexpr inline PkSize &PkSize::operator-=(const PkSize &s) noexcept
{ wd-=s.wd; ht-=s.ht; return *this; }

// qsize.h:172-173 —— 乘 qreal 走 qRound（负半值向 +∞）。**只有 qreal 一个重载**，
// 不像 QPoint 有 float/double/int 三个 —— float 实参会提升到 double 走同一条路。
constexpr inline PkSize &PkSize::operator*=(qreal c) noexcept
{ wd = qRound(wd*c); ht = qRound(ht*c); return *this; }

constexpr inline bool operator==(const PkSize &s1, const PkSize &s2) noexcept
{ return s1.wd == s2.wd && s1.ht == s2.ht; }

constexpr inline bool operator!=(const PkSize &s1, const PkSize &s2) noexcept
{ return s1.wd != s2.wd || s1.ht != s2.ht; }

constexpr inline const PkSize operator+(const PkSize & s1, const PkSize & s2) noexcept
{ return PkSize(s1.wd+s2.wd, s1.ht+s2.ht); }

constexpr inline const PkSize operator-(const PkSize &s1, const PkSize &s2) noexcept
{ return PkSize(s1.wd-s2.wd, s1.ht-s2.ht); }

constexpr inline const PkSize operator*(const PkSize &s, qreal c) noexcept
{ return PkSize(qRound(s.wd*c), qRound(s.ht*c)); }

constexpr inline const PkSize operator*(qreal c, const PkSize &s) noexcept
{ return PkSize(qRound(s.wd*c), qRound(s.ht*c)); }

// qsize.h:193-204 —— Qt 在这两处有 `Q_ASSERT(!qFuzzyIsNull(c))`。**不实现**：
// 断言设施归 R-08，且 Krita 的发布构建里它整条编译掉（Qt5 的 cmake 模块给非
// Debug 构建加 -DQT_NO_DEBUG，见 krita/CMakeLists.txt:968 那条 option 的说明）。
// 对齐的是发布构建的形态：除以 0 得 qRound(±inf)，实测两侧都是 INT_MIN。
// 登记在 README 偏离清单。
inline PkSize &PkSize::operator/=(qreal c)
{
    wd = qRound(wd/c); ht = qRound(ht/c);
    return *this;
}

inline const PkSize operator/(const PkSize &s, qreal c)
{
    return PkSize(qRound(s.wd/c), qRound(s.ht/c));
}

// qsize.h:206-209 —— qMax 逐分量。⚠ qMax 写作 `(a<b)?b:a`，**NaN 上不可交换**，
// 浮点版那边靠这条钉住（这里是整数版，写 qMin 会立刻红）。
constexpr inline PkSize PkSize::expandedTo(const PkSize & otherSize) const noexcept
{
    return PkSize(qMax(wd,otherSize.wd), qMax(ht,otherSize.ht));
}


class PkSizeF
{
public:
    // qsize.h:296 —— ⚠ **(-1.,-1.)**，不是 (0,0)。
    constexpr PkSizeF() noexcept;
    // qsize.h:225 —— **非 explicit**：PkSize 到 PkSizeF 是隐式提升，Task 4/5 的
    // PkRectF 与大量调用点靠这条。
    constexpr PkSizeF(const PkSize &sz) noexcept;
    constexpr PkSizeF(qreal w, qreal h) noexcept;

    // qsize.h:228 —— Qt 这一个**不是** constexpr（qIsNull 在 5.15 才变 constexpr，
    // 声明没跟着改）。照抄，免得 constexpr 上下文里两边可用性不同。
    inline bool isNull() const noexcept;
    constexpr inline bool isEmpty() const noexcept;
    constexpr inline bool isValid() const noexcept;

    constexpr inline qreal width() const noexcept;
    constexpr inline qreal height() const noexcept;
    constexpr inline void setWidth(qreal w) noexcept;
    constexpr inline void setHeight(qreal h) noexcept;

    inline void scale(qreal w, qreal h, Qt::AspectRatioMode mode) noexcept;
    inline void scale(const PkSizeF &s, Qt::AspectRatioMode mode) noexcept;
    PkSizeF scaled(qreal w, qreal h, Qt::AspectRatioMode mode) const noexcept;
    PkSizeF scaled(const PkSizeF &s, Qt::AspectRatioMode mode) const noexcept;

    constexpr inline PkSizeF expandedTo(const PkSizeF &) const noexcept;

    constexpr inline qreal &rwidth() noexcept;
    constexpr inline qreal &rheight() noexcept;

    constexpr inline PkSizeF &operator+=(const PkSizeF &) noexcept;
    constexpr inline PkSizeF &operator-=(const PkSizeF &) noexcept;
    constexpr inline PkSizeF &operator*=(qreal c) noexcept;
    inline PkSizeF &operator/=(qreal c);          // Qt 这一个也没标 noexcept

    friend constexpr inline bool operator==(const PkSizeF &, const PkSizeF &) noexcept;
    friend constexpr inline bool operator!=(const PkSizeF &, const PkSizeF &) noexcept;
    friend constexpr inline const PkSizeF operator+(const PkSizeF &, const PkSizeF &) noexcept;
    friend constexpr inline const PkSizeF operator-(const PkSizeF &, const PkSizeF &) noexcept;
    friend constexpr inline const PkSizeF operator*(const PkSizeF &, qreal) noexcept;
    friend constexpr inline const PkSizeF operator*(qreal, const PkSizeF &) noexcept;
    friend inline const PkSizeF operator/(const PkSizeF &, qreal);

    constexpr inline PkSize toSize() const noexcept;

private:
    qreal wd;
    qreal ht;
};

// ── PkSizeF inline（qsize.h:296-394）──────────────────────────────────────

constexpr inline PkSizeF::PkSizeF() noexcept : wd(-1.), ht(-1.) {}

constexpr inline PkSizeF::PkSizeF(const PkSize &sz) noexcept : wd(sz.width()), ht(sz.height()) {}

constexpr inline PkSizeF::PkSizeF(qreal w, qreal h) noexcept : wd(w), ht(h) {}

// qsize.h:302-303 用的是 qIsNull(wd) && qIsNull(ht)，而 qglobal.h:925-928 的
// qIsNull(double d) 就是 `d == 0.0` —— 于是 **-0.0 也算 null**（实测真 Qt），
// 而 5e-324 不算。这里直接写出那个比较，不把 qIsNull 这个名字提进 compat：
// 它在 Krita 保留范围内实测 0 调用点，导出去就违反判据①「一项不多」。
// 理由与 PkPointF::isNull 完全相同（README 偏离清单第 7 条）。
inline bool PkSizeF::isNull() const noexcept
{ return wd == 0.0 && ht == 0.0; }

// qsize.h:305-309 —— ⚠ 与整数版**不是同一套公式**：门槛是 `<= 0.` 而不是 `< 1`。
// (0.5,0.5) 在这里是**非空**；把整数版抄过来会静默改掉这一整片行为。
// NaN 参与时 `nan <= 0.` 与 `nan >= 0.` 都是 false，于是 (nan,0.5) 既非空也无效。
constexpr inline bool PkSizeF::isEmpty() const noexcept
{ return wd <= 0. || ht <= 0.; }

constexpr inline bool PkSizeF::isValid() const noexcept
{ return wd >= 0. && ht >= 0.; }

constexpr inline qreal PkSizeF::width() const noexcept
{ return wd; }

constexpr inline qreal PkSizeF::height() const noexcept
{ return ht; }

constexpr inline void PkSizeF::setWidth(qreal w) noexcept
{ wd = w; }

constexpr inline void PkSizeF::setHeight(qreal h) noexcept
{ ht = h; }

inline void PkSizeF::scale(qreal w, qreal h, Qt::AspectRatioMode mode) noexcept
{ scale(PkSizeF(w, h), mode); }

inline void PkSizeF::scale(const PkSizeF &s, Qt::AspectRatioMode mode) noexcept
{ *this = scaled(s, mode); }

inline PkSizeF PkSizeF::scaled(qreal w, qreal h, Qt::AspectRatioMode mode) const noexcept
{ return scaled(PkSizeF(w, h), mode); }

constexpr inline qreal &PkSizeF::rwidth() noexcept
{ return wd; }

constexpr inline qreal &PkSizeF::rheight() noexcept
{ return ht; }

constexpr inline PkSizeF &PkSizeF::operator+=(const PkSizeF &s) noexcept
{ wd += s.wd; ht += s.ht; return *this; }

constexpr inline PkSizeF &PkSizeF::operator-=(const PkSizeF &s) noexcept
{ wd -= s.wd; ht -= s.ht; return *this; }

// 浮点版**不取整**（整数版走 qRound）。
constexpr inline PkSizeF &PkSizeF::operator*=(qreal c) noexcept
{ wd *= c; ht *= c; return *this; }

// qsize.h:350-354 —— ⚠ **两个分量各来一次 qFuzzyCompare，没有零分支**。
// 与 QPointF::operator==（任一侧为 0 就改走 fuzzyIsNull）不是同一个写法：
//   QPointF(0,0)==QPointF(1e-300,0) → true
//   QSizeF (0,0)==QSizeF (1e-300,0) → **false**（实测真 Qt 5.15.7）
// 两侧恰好都是 0 时仍相等（0*1e12 <= qMin(0,0) 成立）。
// 走 pkQtFuzzyCompare 而不是 qFuzzyCompare：后者在「pk/test 的垫片先进 TU」这条
// 真实共存路径上是 **#define**，函数体会在预处理期被改写到别人的实现上去
//（理由见 PkGlobal.h 那段注释，探针在 tests/size_macro_proof.cpp）。公式逐字相同。
// operator!= 照抄 Qt 的写法（不是 `!(a==b)`），取值等价。
constexpr inline bool operator==(const PkSizeF &s1, const PkSizeF &s2) noexcept
{ return pkQtFuzzyCompare(s1.wd, s2.wd) && pkQtFuzzyCompare(s1.ht, s2.ht); }

constexpr inline bool operator!=(const PkSizeF &s1, const PkSizeF &s2) noexcept
{ return !pkQtFuzzyCompare(s1.wd, s2.wd) || !pkQtFuzzyCompare(s1.ht, s2.ht); }

constexpr inline const PkSizeF operator+(const PkSizeF & s1, const PkSizeF & s2) noexcept
{ return PkSizeF(s1.wd+s2.wd, s1.ht+s2.ht); }

constexpr inline const PkSizeF operator-(const PkSizeF &s1, const PkSizeF &s2) noexcept
{ return PkSizeF(s1.wd-s2.wd, s1.ht-s2.ht); }

constexpr inline const PkSizeF operator*(const PkSizeF &s, qreal c) noexcept
{ return PkSizeF(s.wd*c, s.ht*c); }

constexpr inline const PkSizeF operator*(qreal c, const PkSizeF &s) noexcept
{ return PkSizeF(s.wd*c, s.ht*c); }

// Q_ASSERT 同上不实现。
inline PkSizeF &PkSizeF::operator/=(qreal c)
{
    wd = wd/c; ht = ht/c;
    return *this;
}

inline const PkSizeF operator/(const PkSizeF &s, qreal c)
{
    return PkSizeF(s.wd/c, s.ht/c);
}

constexpr inline PkSizeF PkSizeF::expandedTo(const PkSizeF & otherSize) const noexcept
{
    return PkSizeF(qMax(wd,otherSize.wd), qMax(ht,otherSize.ht));
}

// qsize.h:391-394 —— qRound，**不是截断**。qRound 对负半值向 +∞ 取整，
// 所以 PkSizeF(-0.5,-0.5).toSize() == (0,0)（实测真 Qt 5.15.7）。
constexpr inline PkSize PkSizeF::toSize() const noexcept
{
    return PkSize(qRound(wd), qRound(ht));
}

#endif // PK_GEOMETRY_PKSIZE_H
