#include "PkRegion.h"

#include <algorithm>

// ---------------------------------------------------------------------------
// PkRegion 的矩形表布尔运算。**不逐位对齐 Qt 的扫描线划分**（见 PkRegion.h 头部
// 与 R-21 plan.md「问 4」的裁决）：只保证覆盖面积正确，不保证划分方式与 Qt 相同。
//
// 所有矩形按**包含边界坐标**（left/top/right/bottom 全含）做运算，与 PkRect
// 的存储（x1/y1/x2/y2，x2/y2 是含边界）一致，避免差一错误。
// ---------------------------------------------------------------------------

namespace {

// 两个矩形是否重叠（含边界相接不算重叠——并运算里相接的相邻矩形由 merge 合并，
// 差运算里相接不做任何事）。
inline bool pkRectsOverlap(const PkRect &a, const PkRect &b)
{
    return a.left() <= b.right() && b.left() <= a.right()
        && a.top() <= b.bottom() && b.top() <= a.bottom();
}

} // namespace

PkRegion::PkRegion() {}

PkRegion::PkRegion(const PkRect &r)
{
    if (!r.isEmpty())
        m_rects.push_back(r);
}

bool PkRegion::isEmpty() const
{
    return m_rects.empty();
}

bool PkRegion::isNull() const
{
    return m_rects.empty();
}

PkRegion::const_iterator PkRegion::begin() const
{
    return m_rects.empty() ? nullptr : m_rects.data();
}

PkRegion::const_iterator PkRegion::end() const
{
    return m_rects.empty() ? nullptr : m_rects.data() + m_rects.size();
}

bool PkRegion::contains(const PkPoint &p) const
{
    for (const PkRect &r : m_rects)
        if (r.contains(p))
            return true;
    return false;
}

bool PkRegion::contains(const PkRect &rect) const
{
    // 覆盖判定：rect 被集合完全覆盖 ⇔ 每个区域矩形与 rect 的交的面积之和 == rect 面积。
    // 区域矩形非重叠 ⇒ 各交也非重叠 ⇒ 面积可加。
    if (rect.isEmpty())
        return true;
    long long covered = 0;
    const long long want = (long long)rect.width() * rect.height();
    for (const PkRect &r : m_rects) {
        const PkRect is = r.intersected(rect);
        if (!is.isEmpty())
            covered += (long long)is.width() * is.height();
    }
    return covered == want;
}

void PkRegion::translate(int dx, int dy)
{
    for (PkRect &r : m_rects)
        r.translate(dx, dy);
}

void PkRegion::translate(const PkPoint &p)
{
    translate(p.x(), p.y());
}

PkRegion PkRegion::translated(int dx, int dy) const
{
    PkRegion result(*this);
    result.translate(dx, dy);
    return result;
}

PkRegion PkRegion::translated(const PkPoint &p) const
{
    return translated(p.x(), p.y());
}

PkRect PkRegion::boundingRect() const
{
    if (m_rects.empty())
        return PkRect();
    PkRect b = m_rects[0];
    for (std::size_t i = 1; i < m_rects.size(); ++i)
        b = b.united(m_rects[i]);
    return b;
}

std::vector<PkRect> PkRegion::rects() const
{
    return m_rects;
}

int PkRegion::rectCount() const
{
    return (int)m_rects.size();
}

// ── 内部：并 / 差 / 交 ─────────────────────────────────────────────────────

void PkRegion::unionRect(const PkRect &r)
{
    if (r.isEmpty())
        return;
    // 先减后加：避免新矩形与既有矩形重叠（重叠会双计面积）。
    subtractRect(r);
    m_rects.push_back(r);
}

void PkRegion::subtractRect(const PkRect &r)
{
    if (r.isEmpty())
        return;
    std::vector<PkRect> out;
    out.reserve(m_rects.size());
    for (const PkRect &rect : m_rects) {
        if (!pkRectsOverlap(rect, r)) {
            out.push_back(rect);
            continue;
        }
        // rect 减 r 至多 4 片（上 / 下 / 左 / 右），全用含边界坐标。
        const int L1 = rect.left(),  T1 = rect.top();
        const int R1 = rect.right(), B1 = rect.bottom();
        const int L2 = r.left(),     T2 = r.top();
        const int R2 = r.right(),    B2 = r.bottom();
        // 上条
        if (T1 < T2)
            out.push_back(PkRect(L1, T1, R1 - L1 + 1, T2 - T1));
        // 下条
        if (B2 < B1)
            out.push_back(PkRect(L1, B2 + 1, R1 - L1 + 1, B1 - B2));
        // 左右条（限于重叠的纵向范围）
        const int yTop = std::max(T1, T2);
        const int yBot = std::min(B1, B2);
        if (yTop <= yBot) {
            const int h = yBot - yTop + 1;
            if (L1 < L2)
                out.push_back(PkRect(L1, yTop, L2 - L1, h));
            if (R2 < R1)
                out.push_back(PkRect(R2 + 1, yTop, R1 - R2, h));
        }
    }
    m_rects = std::move(out);
    merge();
}

void PkRegion::intersectRect(const PkRect &r)
{
    std::vector<PkRect> out;
    for (const PkRect &rect : m_rects) {
        const PkRect is = rect.intersected(r);
        if (!is.isEmpty())
            out.push_back(is);
    }
    m_rects = std::move(out);
    merge();
}

// ── 相邻合并（水平 + 垂直两趟，同 KisRegion::mergeSparseRects 的精神）────────
//
// 只合并**恰好相邻**的矩形（一条边重合、不重叠），不合并重叠的（重叠在并运算
// 里已经被「先减后加」消掉，这里不会见到）。合并只为压 rectCount，不是覆盖
// 正确性的前提。
void PkRegion::merge()
{
    if (m_rects.size() < 2)
        return;
    // 水平合并：同一 top、同一 height、且 right+1 == 左邻 left。
    for (;;) {
        bool changed = false;
        std::sort(m_rects.begin(), m_rects.end(),
                  [](const PkRect &a, const PkRect &b) {
                      if (a.top() != b.top()) return a.top() < b.top();
                      if (a.height() != b.height()) return a.height() < b.height();
                      return a.left() < b.left();
                  });
        std::vector<PkRect> out;
        for (const PkRect &r : m_rects) {
            if (!out.empty()) {
                PkRect &back = out.back();
                if (back.top() == r.top() && back.height() == r.height()
                    && back.right() + 1 == r.left()) {
                    back.setRight(r.right());
                    changed = true;
                    continue;
                }
            }
            out.push_back(r);
        }
        m_rects = std::move(out);
        if (!changed)
            break;
    }
    // 垂直合并：同一 left、同一 width、且 bottom+1 == 上邻 top。
    for (;;) {
        bool changed = false;
        std::sort(m_rects.begin(), m_rects.end(),
                  [](const PkRect &a, const PkRect &b) {
                      if (a.left() != b.left()) return a.left() < b.left();
                      if (a.width() != b.width()) return a.width() < b.width();
                      return a.top() < b.top();
                  });
        std::vector<PkRect> out;
        for (const PkRect &r : m_rects) {
            if (!out.empty()) {
                PkRect &back = out.back();
                if (back.left() == r.left() && back.width() == r.width()
                    && back.bottom() + 1 == r.top()) {
                    back.setBottom(r.bottom());
                    changed = true;
                    continue;
                }
            }
            out.push_back(r);
        }
        m_rects = std::move(out);
        if (!changed)
            break;
    }
}

// ── 复合赋值 ───────────────────────────────────────────────────────────────

PkRegion &PkRegion::operator|=(const PkRegion &r)
{
    for (const PkRect &rc : r.m_rects)
        unionRect(rc);
    merge();
    return *this;
}

PkRegion &PkRegion::operator+=(const PkRegion &r)
{
    return *this |= r;
}

PkRegion &PkRegion::operator+=(const PkRect &r)
{
    unionRect(r);
    merge();
    return *this;
}

PkRegion &PkRegion::operator&=(const PkRegion &r)
{
    // 空区域 ∩ 任何 = 空（对拍实锤：QRegion(rect).intersected(QRegion()) 是空）。
    if (r.m_rects.empty()) {
        m_rects.clear();
        return *this;
    }
    // 顺序交：*this ∩= rc 对 r 的每个矩形做一遍（交是可结合的，逐矩形收窄正确）。
    for (const PkRect &rc : r.m_rects)
        intersectRect(rc);
    return *this;
}

PkRegion &PkRegion::operator&=(const PkRect &r)
{
    intersectRect(r);
    return *this;
}

PkRegion &PkRegion::operator-=(const PkRegion &r)
{
    for (const PkRect &rc : r.m_rects)
        subtractRect(rc);
    merge();
    return *this;
}

PkRegion &PkRegion::operator-=(const PkRect &r)
{
    subtractRect(r);
    return *this;
}

PkRegion &PkRegion::operator^=(const PkRegion &r)
{
    const PkRegion a = *this - r;
    const PkRegion b = r - *this;
    *this = a;
    *this |= b;
    return *this;
}

// ── 自由运算符（返回新对象）────────────────────────────────────────────────

PkRegion PkRegion::operator|(const PkRegion &r) const
{
    PkRegion result(*this);
    result |= r;
    return result;
}

PkRegion PkRegion::operator+(const PkRegion &r) const
{
    return *this | r;
}

PkRegion PkRegion::operator+(const PkRect &r) const
{
    PkRegion result(*this);
    result += r;
    return result;
}

PkRegion PkRegion::operator&(const PkRegion &r) const
{
    PkRegion result(*this);
    result &= r;
    return result;
}

PkRegion PkRegion::operator&(const PkRect &r) const
{
    PkRegion result(*this);
    result &= r;
    return result;
}

PkRegion PkRegion::operator-(const PkRegion &r) const
{
    PkRegion result(*this);
    result -= r;
    return result;
}

PkRegion PkRegion::operator-(const PkRect &r) const
{
    PkRegion result(*this);
    result -= r;
    return result;
}

PkRegion PkRegion::operator^(const PkRegion &r) const
{
    PkRegion result(*this);
    result ^= r;
    return result;
}

// ── 具名成员 ───────────────────────────────────────────────────────────────

PkRegion PkRegion::united(const PkRegion &r) const
{
    return *this | r;
}

PkRegion PkRegion::united(const PkRect &r) const
{
    return *this + r;
}

PkRegion PkRegion::intersected(const PkRegion &r) const
{
    return *this & r;
}

PkRegion PkRegion::intersected(const PkRect &r) const
{
    return *this & r;
}

PkRegion PkRegion::subtracted(const PkRegion &r) const
{
    return *this - r;
}

PkRegion PkRegion::xored(const PkRegion &r) const
{
    return *this ^ r;
}

bool PkRegion::intersects(const PkRegion &r) const
{
    for (const PkRect &a : m_rects)
        for (const PkRect &b : r.m_rects)
            if (pkRectsOverlap(a, b))
                return true;
    return false;
}

bool PkRegion::intersects(const PkRect &r) const
{
    for (const PkRect &a : m_rects)
        if (pkRectsOverlap(a, r))
            return true;
    return false;
}

bool PkRegion::operator==(const PkRegion &r) const
{
    if (m_rects.size() != r.m_rects.size())
        return false;
    // 覆盖相等：面积相同 + 逐点采样等价太贵，这里用「对称差为空」判覆盖相等，
    // 这才是 QRegion::operator== 的语义（Qt 文档：returns true if the region is
    // equal to r，即覆盖区域相同）。但我们的划分可能与 Qt 不同，两侧各自做对称差。
    return (*this - r).isEmpty() && (r - *this).isEmpty();
}

bool PkRegion::operator!=(const PkRegion &r) const
{
    return !(*this == r);
}
