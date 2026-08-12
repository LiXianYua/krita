// difftest_set.cpp —— QSet<T> ↔ PkSet<T> 对拍
//
// 输入模型同 difftest_seq.cpp。QSet 的迭代顺序在 Qt 里就未定义（且带随机化种子，
// 跨补丁版本会变），所以 dump 与 `values()` 都**先收集再排序**，「顺序」这一维
// 完全没有覆盖。
//
// 集合运算（unite / intersect / subtract）额外做了**自参数**形态：
// `s.subtract(s)` / `s.intersect(s)` / `s.unite(s)`。这三条是刻意压的对抗输入 ——
// 实现里若是「先取 other 的引用、再对自己 PkMut() 边读边删」，自参数下就是
// 迭代器失效。Qt 那边 `QSet::subtract` 显式先拷两份再删，所以它有定义。

// `qHash` 改名的完整理由见 difftest_assoc.cpp 顶部：`PkHashFunctions.h` 与真 Qt
// 的 `qhashfunctions.h` 对内建类型定义了逐字同签名的全局 `qHash` 重载，同一个
// 翻译单元里放不下两份。宏只作用于其后扫描到的替代品头文件。
#include <QSet>
#include <QList>
#include <QString>

#define qHash pkOracleHash
#include <PkSet.h>
#include <PkStringHash.h>   // qHash(const PkString &) —— PkSet<PkString> 要它
#undef qHash

#include "difftest_common.h"

static_assert(std::is_same<decltype(qHash(QString())), uint>::value,
              "qHash 改名误伤了真 Qt 那一侧");

#include <string>
#include <type_traits>
#include <vector>

static_assert(!std::is_same<QSet<int>, PkSet<int>>::value, "QSet/PkSet 编成了同一类型");

enum SetKind {
    S_INSERT = 0, S_REMOVE, S_CONTAINS, S_SIZE, S_COUNT, S_ISEMPTY, S_CLEAR,
    S_VALUES, S_UNITE, S_INTERSECT, S_SUBTRACT, S_OR_ASSIGN, S_EQ_OTHER,
    S_ASSIGN_SELF, S_UNITE_SELF, S_INTERSECT_SELF, S_SUBTRACT_SELF,
};

struct SetOp {
    int kind;
    int t;    // 元素 token 下标
    int t2;   // 构造 other 的第二个 token
};

static SetOp mk(int kind, int t = 0, int t2 = 1)
{
    SetOp op; op.kind = kind; op.t = t; op.t2 = t2; return op;
}

struct SetPlan {
    bool run = true;
    const char *api = "?";
    std::string shape;
};

// 形态由「这个元素在不在集合里 / 集合空不空 / other 与自身的交叠程度」算出来。
static SetPlan planSet(const SetOp &op, int n, bool present, int overlap, int otherSize)
{
    SetPlan pl;
    const char *ov = (otherSize == 0) ? "other-empty"
                   : (overlap == 0) ? "disjoint"
                   : (overlap == otherSize) ? "other-subset" : "partial-overlap";
    switch (op.kind) {
    case S_INSERT:   pl.api = "insert";   pl.shape = present ? "dup-key" : "uniq-key"; break;
    case S_REMOVE:   pl.api = "remove";   pl.shape = pkPresentShape(present); break;
    case S_CONTAINS: pl.api = "contains"; pl.shape = pkPresentShape(present); break;
    case S_SIZE:     pl.api = "size";     pl.shape = pkEmptyShape(n); break;
    case S_COUNT:    pl.api = "count";    pl.shape = pkEmptyShape(n); break;
    case S_ISEMPTY:  pl.api = "isEmpty";  pl.shape = pkEmptyShape(n); break;
    case S_CLEAR:    pl.api = "clear";    pl.shape = pkEmptyShape(n); break;
    case S_VALUES:   pl.api = "values";   pl.shape = pkEmptyShape(n); break;
    case S_UNITE:     pl.api = "unite";      pl.shape = std::string(pkEmptyShape(n)) + "-" + ov; break;
    case S_INTERSECT: pl.api = "intersect";  pl.shape = std::string(pkEmptyShape(n)) + "-" + ov; break;
    case S_SUBTRACT:  pl.api = "subtract";   pl.shape = std::string(pkEmptyShape(n)) + "-" + ov; break;
    case S_OR_ASSIGN: pl.api = "operator|=";  pl.shape = std::string(pkEmptyShape(n)) + "-" + ov; break;
    case S_EQ_OTHER:  pl.api = "operator==";  pl.shape = std::string(pkEmptyShape(n)) + "-" + ov; break;
    case S_ASSIGN_SELF:    pl.api = "assign-self";    pl.shape = pkEmptyShape(n); break;
    case S_UNITE_SELF:     pl.api = "unite-self";     pl.shape = pkEmptyShape(n); break;
    case S_INTERSECT_SELF: pl.api = "intersect-self"; pl.shape = pkEmptyShape(n); break;
    case S_SUBTRACT_SELF:  pl.api = "subtract-self";  pl.shape = pkEmptyShape(n); break;
    default: pl.run = false; break;
    }
    return pl;
}

template <typename S, typename T>
static S mkOther(const SetOp &op)
{
    S s;
    s.insert(PkMake<T>::make(op.t));
    s.insert(PkMake<T>::make(op.t2));
    return s;
}

template <typename S, typename T>
static std::string execSet(S &s, const SetOp &op)
{
    const T v = PkMake<T>::make(op.t);
    switch (op.kind) {
    case S_INSERT:   { auto it = s.insert(v); return es(*it); }
    case S_REMOVE:   return es(s.remove(v));
    case S_CONTAINS: return es(s.contains(v));
    case S_SIZE:     return es(s.size());
    case S_COUNT:    return es(s.count());
    case S_ISEMPTY:  return es(s.isEmpty());
    case S_CLEAR:    s.clear(); return "-";
    case S_VALUES:   return dumpListSorted(s.values());
    case S_UNITE:     { const S o = mkOther<S, T>(op); s.unite(o);     return "-"; }
    case S_INTERSECT: { const S o = mkOther<S, T>(op); s.intersect(o); return "-"; }
    case S_SUBTRACT:  { const S o = mkOther<S, T>(op); s.subtract(o);  return "-"; }
    case S_OR_ASSIGN: { const S o = mkOther<S, T>(op); s |= o;         return "-"; }
    case S_EQ_OTHER:  { const S o = mkOther<S, T>(op); return es(s == o); }
    case S_ASSIGN_SELF:    { const S *self = &s; s = *self;         return "-"; }
    case S_UNITE_SELF:     { const S *self = &s; s.unite(*self);     return "-"; }
    case S_INTERSECT_SELF: { const S *self = &s; s.intersect(*self); return "-"; }
    case S_SUBTRACT_SELF:  { const S *self = &s; s.subtract(*self);  return "-"; }
    default: break;
    }
    return "??";
}

template <typename QS, typename PS, typename QT, typename PT>
static void runSetSuite(const char *suite, bool elemStr,
                        const std::vector<SetOp> &instrs,
                        const std::vector<int> &mutIdx,
                        const std::vector<std::vector<int>> &inits)
{
    pkForEachCase(static_cast<int>(instrs.size()), mutIdx, inits,
        [&](const std::vector<int> &seq, const std::vector<int> &init, bool shared) {
            QS q;
            PS p;
            for (int t : init) {
                q.insert(PkMake<QT>::make(t));
                p.insert(PkMake<PT>::make(t));
            }
            QS qa;
            PS pa;
            if (shared) { qa = q; pa = p; }

            std::string ctx = "init=" + dumpSetSorted(q);

            for (std::size_t k = 0; k < seq.size(); ++k) {
                const SetOp &op = instrs[seq[k]];
                const QT tv = PkMake<QT>::make(op.t);
                const QT tv2 = PkMake<QT>::make(op.t2);
                const bool present = q.contains(tv);

                // other 恒是 {t, t2}（两个 token 相同则退化成单元素）
                int otherSize = (es(tv) == es(tv2)) ? 1 : 2;
                int overlap = (present ? 1 : 0) + ((otherSize == 2 && q.contains(tv2)) ? 1 : 0);

                const SetPlan pl = planSet(op, q.size(), present, overlap, otherSize);
                if (!pl.run) {
                    continue;
                }

                const std::string qret = execSet<QS, QT>(q, op);
                const std::string pret = execSet<PS, PT>(p, op);
                const std::string qd = dumpSetSorted(q);
                const std::string pd = dumpSetSorted(p);
                std::string qad, pad;
                if (shared) { qad = dumpSetSorted(qa); pad = dumpSetSorted(pa); }

                const bool same = (qret == pret) && (qd == pd) && (qad == pad);
                rec(std::string(suite) + "." + pl.api, same,
                    pkTag(pl.shape, shared, elemStr),
                    ctx + " ; " + pl.api + "(t=" + es(tv) + ",t2=" + es(tv2) + ")",
                    qret + " " + qd + (shared ? " alias=" + qad : std::string()),
                    pret + " " + pd + (shared ? " alias=" + pad : std::string()));

                if (qd != pd || qad != pad) {
                    break;
                }
                ctx += " ; " + std::string(pl.api);
            }
        });
}

static const std::vector<std::vector<int>> kInits = {
    {},
    { 1 },
    { 1, 1 },               // 插重复 → 集合仍是单元素
    { 0, 1, 2 },
    { 0, 1, 2, 3, 4, 5, 6 },
    { 4, 5, 6 },
};

static std::vector<SetOp> buildInstrs()
{
    std::vector<SetOp> v;
    for (int t : { 0, 1, 3, 6 }) {
        v.push_back(mk(S_INSERT, t));
        v.push_back(mk(S_REMOVE, t));
        v.push_back(mk(S_CONTAINS, t));
    }
    for (int t : { 0, 1, 6 }) {
        for (int t2 : { 1, 2, 6 }) {
            v.push_back(mk(S_UNITE, t, t2));
            v.push_back(mk(S_INTERSECT, t, t2));
            v.push_back(mk(S_SUBTRACT, t, t2));
            v.push_back(mk(S_OR_ASSIGN, t, t2));
            v.push_back(mk(S_EQ_OTHER, t, t2));
        }
    }
    v.push_back(mk(S_SIZE));
    v.push_back(mk(S_COUNT));
    v.push_back(mk(S_ISEMPTY));
    v.push_back(mk(S_CLEAR));
    v.push_back(mk(S_VALUES));
    v.push_back(mk(S_ASSIGN_SELF));
    v.push_back(mk(S_UNITE_SELF));
    v.push_back(mk(S_INTERSECT_SELF));
    v.push_back(mk(S_SUBTRACT_SELF));
    return v;
}

static bool isMutating(const SetOp &op)
{
    switch (op.kind) {
    case S_CONTAINS: case S_SIZE: case S_COUNT: case S_ISEMPTY:
    case S_VALUES: case S_EQ_OTHER:
        return false;
    default:
        return true;
    }
}

static std::vector<int> mutSubset(const std::vector<SetOp> &v)
{
    std::vector<int> out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (isMutating(v[i]) && (out.size() * 4 <= i)) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

int main()
{
    oracleBegin("difftest_set");

    const std::vector<SetOp> instrs = buildInstrs();
    const std::vector<int> ms = mutSubset(instrs);

    runSetSuite<QSet<int>, PkSet<int>, int, int>("set", false, instrs, ms, kInits);
    runSetSuite<QSet<QString>, PkSet<PkString>, QString, PkString>("set", true, instrs, ms, kInits);

    return oracleEnd();
}
