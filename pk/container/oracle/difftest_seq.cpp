// difftest_seq.cpp —— 序列容器族与真 Qt 5 对拍
//
//   QVector<T>     ↔ PkVector<T>
//   QList<T>       ↔ PkList<T>
//   QStringList    ↔ PkStringList
//   QStack<T>      ↔ PkStack<T>
//   QQueue<T>      ↔ PkQueue<T>
//
// 输入模型是**指令 VM**：一次用例 =（元素类型, 初始元素序列, 操作序列, 共享与否）。
// 两侧各建一个容器，执行同一串操作，每执行一条比三样东西——该操作的返回值、
// 操作后自身的 dump、（共享用例里）alias 的 dump。三样都同才算这一步 same。
// 详见 difftest_common.h 顶部。
//
// ── 哪些调用**故意不生成** ────────────────────────────────
// 越界 `at()` / `operator[]()` / 空容器 `first()`/`last()`/`takeFirst()`：
// **理由不是「会崩」，是 Qt 在这里没有定义行为。** 本机真 Qt 5.15.7 实测：
//
//   默认构建(Q_ASSERT 生效)   at(5) / [](5) / at(-1) / 空容器 first()  → rc=134 abort
//   -DQT_NO_DEBUG (release)   同样四项                                → rc=0，返回 0，活着跑完
//
// release 下不崩**比崩更麻烦**：两侧各自返回不同的垃圾值会产生虚假 mismatch，
// 而那是 Qt 未定义的格子，不该被记成差异、更不该为它写一条 deviation。
// 所以 planSeq() 里这些指令带 `run=false` 的守卫，**输入集里根本不生成它们**。
// 可以对拍的越界形态由 `value(i)` / `value(i,def)` / `indexOf(t,from)` /
// `lastIndexOf(t,from)` 承担 —— 这四个在 Qt 里对任意下标都有定义。

#include <QVector>
#include <QList>
#include <QStringList>
#include <QStack>
#include <QQueue>
#include <QString>

#include <PkVector.h>
#include <PkList.h>
#include <PkStringList.h>
#include <PkStack.h>
#include <PkQueue.h>

#include "difftest_common.h"

#include <string>
#include <type_traits>
#include <vector>

// ── 形态契约：两侧真的是不同类型 ──────────────────────────
static_assert(!std::is_same<QVector<int>, PkVector<int>>::value, "QVector/PkVector 编成了同一类型");
static_assert(!std::is_same<QList<int>, PkList<int>>::value, "QList/PkList 编成了同一类型");
static_assert(!std::is_same<QStringList, PkStringList>::value, "QStringList/PkStringList 编成了同一类型");
static_assert(!std::is_same<QStack<int>, PkStack<int>>::value, "QStack/PkStack 编成了同一类型");
static_assert(!std::is_same<QQueue<int>, PkQueue<int>>::value, "QQueue/PkQueue 编成了同一类型");

// ── 指令集 ────────────────────────────────────────────────

enum SeqKind {
    K_APPEND = 0, K_PREPEND, K_INSERT, K_CLEAR, K_VALUE, K_VALUE_DEF,
    K_INDEXOF, K_LASTINDEXOF, K_CONTAINS, K_COUNT_T, K_SIZE, K_ISEMPTY,
    K_AT, K_FIRST, K_LAST, K_ERASE_AT, K_APPEND_OTHER, K_APPEND_SELF,
    K_ASSIGN_SELF, K_RESERVE, K_SHL, K_EQ_OTHER,
    // QVector 专有
    K_VREMOVE, K_VREMOVE_N, K_RESIZE, K_FILL, K_FILL_N,
    // QList 专有
    K_REMOVEAT, K_REMOVEALL, K_REMOVEONE, K_REMOVEFIRST, K_REMOVELAST,
    K_TAKEAT, K_TAKEFIRST, K_TAKELAST, K_MOVE,
    // QStack / QQueue 专有
    K_PUSH, K_POP, K_TOP, K_ENQUEUE, K_DEQUEUE, K_HEAD,
};

struct SeqOp {
    int kind;
    int a;    // 下标 1（可以是 PK_IDX_* 编码）
    int b;    // 下标 2 / 长度
    int t;    // 元素 token 下标
    int t2;   // 元素 token 下标 2
};

static SeqOp mk(int kind, int a = 0, int b = 0, int t = 0, int t2 = 1)
{
    SeqOp op; op.kind = kind; op.a = a; op.b = b; op.t = t; op.t2 = t2; return op;
}

// ── 规划：形态与守卫 ──────────────────────────────────────
//
// **形态一律由「这条输入相对当前状态落在哪」算出来**，不是由 API 名字算出来的
// （规则一）。守卫按 Qt 侧的当前状态判，两侧因此走同一条控制流；一旦状态岔开，
// 调用方会立即中止这个用例，不会拿岔开后的 size 再去算守卫。

struct SeqPlan {
    bool run = true;
    const char *api = "?";
    std::string shape;
    int a = 0;
    int b = 0;
};

// 元素在容器里出现几次 —— 「重复元素」这一维
static std::string occShape(const std::vector<std::string> &elems, const std::string &tok)
{
    long n = 0;
    for (const auto &e : elems) {
        if (e == tok) { ++n; }
    }
    return n == 0 ? "absent" : (n == 1 ? "uniq" : "dup");
}

static SeqPlan planSeq(const SeqOp &op, const std::vector<std::string> &elems,
                       const std::string &tok)
{
    const int n = static_cast<int>(elems.size());
    SeqPlan pl;
    pl.a = pkResolveIdx(op.a, n);
    pl.b = pkResolveIdx(op.b, n);

    switch (op.kind) {
    case K_APPEND:       pl.api = "append";       pl.shape = pkEmptyShape(n); break;
    case K_PREPEND:      pl.api = "prepend";      pl.shape = pkEmptyShape(n); break;
    case K_SHL:          pl.api = "operator<<";   pl.shape = pkEmptyShape(n); break;
    case K_CLEAR:        pl.api = "clear";        pl.shape = pkEmptyShape(n); break;
    case K_SIZE:         pl.api = "size";         pl.shape = pkEmptyShape(n); break;
    case K_ISEMPTY:      pl.api = "isEmpty";      pl.shape = pkEmptyShape(n); break;
    case K_ASSIGN_SELF:  pl.api = "assign-self";  pl.shape = pkEmptyShape(n); break;
    case K_APPEND_SELF:  pl.api = "append-self";  pl.shape = pkEmptyShape(n); break;
    case K_APPEND_OTHER: pl.api = "append-other"; pl.shape = pkEmptyShape(n); break;
    case K_EQ_OTHER:     pl.api = "operator==";   pl.shape = pkEmptyShape(n); break;

    case K_INSERT:
        pl.api = "insert";
        pl.shape = pkIdxShape(pl.a, n);
        // Qt：insert(i) 要求 0 <= i <= size，越界是 Q_ASSERT / UB
        pl.run = (pl.a >= 0 && pl.a <= n);
        break;

    // 这四个在 Qt 里对**任意**下标都有定义 —— 越界形态全靠它们承担
    case K_VALUE:      pl.api = "value";       pl.shape = pkIdxShape(pl.a, n); break;
    case K_VALUE_DEF:  pl.api = "value-def";   pl.shape = pkIdxShape(pl.a, n); break;
    case K_INDEXOF:
        pl.api = "indexOf";
        pl.shape = occShape(elems, tok) + "-from-" + pkIdxShape(pl.a, n);
        break;
    case K_LASTINDEXOF:
        pl.api = "lastIndexOf";
        pl.shape = occShape(elems, tok) + "-from-" + pkIdxShape(pl.a, n);
        break;

    case K_CONTAINS: pl.api = "contains"; pl.shape = occShape(elems, tok); break;
    case K_COUNT_T:  pl.api = "count-t";  pl.shape = occShape(elems, tok); break;

    case K_AT:
        pl.api = "at";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0 && pl.a < n);   // 越界 at() 在 Qt 里没有定义行为
        break;
    case K_ERASE_AT:
        pl.api = "erase-at";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0 && pl.a < n);
        break;

    case K_FIRST: pl.api = "first"; pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_LAST:  pl.api = "last";  pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;

    case K_RESERVE:
        pl.api = "reserve";
        pl.shape = pkEmptyShape(n);
        // 负数 reserve 不生成：Pk 侧会把负数转成 size_t 去要 2^64 字节。
        pl.run = (pl.a >= 0);
        break;

    // ---- QVector 专有 ----
    case K_VREMOVE:
        pl.api = "remove";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0 && pl.a < n);
        break;
    case K_VREMOVE_N:
        pl.api = "remove-n";
        pl.shape = std::string(pkIdxShape(pl.a, n)) + "-len" + std::to_string(op.b);
        pl.run = (pl.a >= 0 && op.b >= 0 && pl.a + op.b <= n);
        pl.b = op.b;
        break;
    case K_RESIZE:
        pl.api = "resize";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0);
        break;
    case K_FILL:   pl.api = "fill";   pl.shape = pkEmptyShape(n); break;
    case K_FILL_N:
        pl.api = "fill-n";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0);
        break;

    // ---- QList 专有 ----
    case K_REMOVEAT:
        pl.api = "removeAt";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0 && pl.a < n);
        break;
    case K_TAKEAT:
        pl.api = "takeAt";
        pl.shape = pkIdxShape(pl.a, n);
        pl.run = (pl.a >= 0 && pl.a < n);
        break;
    case K_REMOVEALL: pl.api = "removeAll"; pl.shape = occShape(elems, tok); break;
    case K_REMOVEONE: pl.api = "removeOne"; pl.shape = occShape(elems, tok); break;
    case K_REMOVEFIRST: pl.api = "removeFirst"; pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_REMOVELAST:  pl.api = "removeLast";  pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_TAKEFIRST:   pl.api = "takeFirst";   pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_TAKELAST:    pl.api = "takeLast";    pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_MOVE:
        pl.api = "move";
        pl.shape = std::string(pkIdxShape(pl.a, n)) + "-to-" + pkIdxShape(pl.b, n);
        pl.run = (pl.a >= 0 && pl.a < n && pl.b >= 0 && pl.b < n);
        break;

    // ---- QStack / QQueue 专有 ----
    case K_PUSH:    pl.api = "push";    pl.shape = pkEmptyShape(n); break;
    case K_ENQUEUE: pl.api = "enqueue"; pl.shape = pkEmptyShape(n); break;
    case K_POP:     pl.api = "pop";     pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_TOP:     pl.api = "top";     pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_DEQUEUE: pl.api = "dequeue"; pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;
    case K_HEAD:    pl.api = "head";    pl.shape = pkEmptyShape(n); pl.run = (n > 0); break;

    default: pl.run = false; break;
    }
    return pl;
}

// ── 执行：同一份模板分别用 Qt 侧与 Pk 侧类型实例化 ────────
//
// 两侧类型不同但方法名相同（这正是「API 形状与 Qt 一致」的意义），所以一份模板
// 能同时吃下。**这也顺带验证了形状对齐：模板编不过就说明某个方法签名与 Qt 不一致。**
// 返回值编成字符串；无返回值的返回 "-"。

template <typename C, typename T>
static C mkOther(const SeqOp &op)
{
    C other;
    other.append(PkMake<T>::make(op.t));
    other.append(PkMake<T>::make(op.t2));
    return other;
}

template <typename C, typename T>
static std::string execCore(C &c, const SeqOp &op, int a, int b)
{
    const T v = PkMake<T>::make(op.t);
    switch (op.kind) {
    case K_APPEND:  c.append(v);  return "-";
    case K_PREPEND: c.prepend(v); return "-";
    case K_SHL:     c << v;       return "-";
    case K_INSERT:  c.insert(a, v); return "-";
    case K_CLEAR:   c.clear();    return "-";
    case K_SIZE:    return es(c.size());
    case K_ISEMPTY: return es(c.isEmpty());
    case K_VALUE:     return es(c.value(a));
    case K_VALUE_DEF: return es(c.value(a, v));
    case K_INDEXOF:      return es(c.indexOf(v, a));
    case K_LASTINDEXOF:  return es(c.lastIndexOf(v, a));
    case K_CONTAINS: return es(c.contains(v));
    case K_COUNT_T:  return es(c.count(v));
    case K_AT:    return es(c.at(a));
    case K_FIRST: return es(c.first());
    case K_LAST:  return es(c.last());
    case K_ERASE_AT: c.erase(c.begin() + a); return "-";
    case K_RESERVE:  c.reserve(a); return "-";
    case K_APPEND_OTHER: { const C other = mkOther<C, T>(op); c.append(other); return "-"; }
    case K_APPEND_SELF:  { const C *self = &c; c.append(*self); return "-"; }
    case K_ASSIGN_SELF:  { const C *self = &c; c = *self; return "-"; }
    case K_EQ_OTHER:     { const C other = mkOther<C, T>(op); return es(c == other); }
    default: break;
    }
    (void)b;
    return "??";
}

struct VecPolicy {
    template <typename C, typename T>
    static std::string exec(C &c, const SeqOp &op, int a, int b)
    {
        const T v = PkMake<T>::make(op.t);
        switch (op.kind) {
        case K_VREMOVE:   c.remove(a);    return "-";
        case K_VREMOVE_N: c.remove(a, b); return "-";
        case K_RESIZE:    c.resize(a);    return "-";
        case K_FILL:      c.fill(v);      return "-";
        case K_FILL_N:    c.fill(v, a);   return "-";
        default: return execCore<C, T>(c, op, a, b);
        }
    }
};

struct ListPolicy {
    template <typename C, typename T>
    static std::string exec(C &c, const SeqOp &op, int a, int b)
    {
        const T v = PkMake<T>::make(op.t);
        switch (op.kind) {
        case K_REMOVEAT:    c.removeAt(a);    return "-";
        case K_REMOVEALL:   return es(c.removeAll(v));
        case K_REMOVEONE:   return es(c.removeOne(v));
        case K_REMOVEFIRST: c.removeFirst();  return "-";
        case K_REMOVELAST:  c.removeLast();   return "-";
        case K_TAKEAT:      return es(c.takeAt(a));
        case K_TAKEFIRST:   return es(c.takeFirst());
        case K_TAKELAST:    return es(c.takeLast());
        case K_MOVE:        c.move(a, b);     return "-";
        default: return execCore<C, T>(c, op, a, b);
        }
    }
};

struct StackPolicy {
    template <typename C, typename T>
    static std::string exec(C &c, const SeqOp &op, int a, int b)
    {
        const T v = PkMake<T>::make(op.t);
        switch (op.kind) {
        case K_PUSH: c.push(v);   return "-";
        case K_POP:  return es(c.pop());
        case K_TOP:  return es(c.top());
        default: return VecPolicy::exec<C, T>(c, op, a, b);
        }
    }
};

struct QueuePolicy {
    template <typename C, typename T>
    static std::string exec(C &c, const SeqOp &op, int a, int b)
    {
        const T v = PkMake<T>::make(op.t);
        switch (op.kind) {
        case K_ENQUEUE: c.enqueue(v); return "-";
        case K_DEQUEUE: return es(c.dequeue());
        case K_HEAD:    return es(c.head());
        default: return ListPolicy::exec<C, T>(c, op, a, b);
        }
    }
};

// ── 用例执行 ──────────────────────────────────────────────

template <typename QC, typename PC, typename QT, typename PT, typename Policy>
static void runSeqSuite(const char *suite, bool elemStr,
                        const std::vector<SeqOp> &instrs,
                        const std::vector<int> &mutIdx,
                        const std::vector<std::vector<int>> &inits)
{
    pkForEachCase(static_cast<int>(instrs.size()), mutIdx, inits,
        [&](const std::vector<int> &seq, const std::vector<int> &init, bool shared) {
            QC q;
            PC p;
            for (int t : init) {
                q.append(PkMake<QT>::make(t));
                p.append(PkMake<PT>::make(t));
            }
            // 共享用例：先拷一份 alias，任何写操作都要 detach。COW 破了（写穿到
            // alias）会在 alias 的 dump 上现形，并落在**破它的那条操作**的 tag 上。
            QC qa;
            PC pa;
            if (shared) { qa = q; pa = p; }

            std::string ctx = "init=" + dumpSeq(q);

            for (std::size_t k = 0; k < seq.size(); ++k) {
                const SeqOp &op = instrs[seq[k]];

                std::vector<std::string> elems;
                elems.reserve(static_cast<std::size_t>(q.size()));
                for (int i = 0; i < q.size(); ++i) {
                    elems.push_back(es(q.at(i)));
                }
                const std::string tok = es(PkMake<QT>::make(op.t));

                const SeqPlan pl = planSeq(op, elems, tok);
                if (!pl.run) {
                    continue;
                }

                const std::string qret = Policy::template exec<QC, QT>(q, op, pl.a, pl.b);
                const std::string pret = Policy::template exec<PC, PT>(p, op, pl.a, pl.b);
                const std::string qd = dumpSeq(q);
                const std::string pd = dumpSeq(p);
                std::string qad, pad;
                if (shared) {
                    qad = dumpSeq(qa);
                    pad = dumpSeq(pa);
                }

                const bool same = (qret == pret) && (qd == pd) && (qad == pad);
                rec(std::string(suite) + "." + pl.api, same,
                    pkTag(pl.shape, shared, elemStr),
                    ctx + " ; " + pl.api + "(a=" + std::to_string(pl.a)
                        + ",b=" + std::to_string(pl.b) + ",t=" + tok + ")",
                    qret + " " + qd + (shared ? " alias=" + qad : std::string()),
                    pret + " " + pd + (shared ? " alias=" + pad : std::string()));

                // 状态已经岔开：后续操作的守卫会按不同的 size 算，tag 会张冠李戴。
                if (qd != pd || qad != pad) {
                    break;
                }
                ctx += " ; " + std::string(pl.api);
            }
        });
}

// ── 初始状态：手挑的对抗形态 ──────────────────────────────

static const std::vector<std::vector<int>> kInits = {
    {},                 // 空容器
    { 0 },              // 单元素
    { 1, 1 },           // 全重复
    { 0, 1, 2, 1 },     // 中间有重复
    { 0, 1, 2, 3, 4, 5 },   // 长序列（含全部 token）
};

// ── 指令表 ────────────────────────────────────────────────
//
// 下标一律覆盖负数 / 0 / 中间 / size-1 / size / size+1 —— R-01 那批此前无人知道的
// 不一致**全出在负数与越界参数上**。

static void addCommon(std::vector<SeqOp> &v)
{
    v.push_back(mk(K_APPEND, 0, 0, 0));
    v.push_back(mk(K_APPEND, 0, 0, 4));
    v.push_back(mk(K_PREPEND, 0, 0, 1));
    v.push_back(mk(K_SHL, 0, 0, 2));
    v.push_back(mk(K_INSERT, 0, 0, 3));
    v.push_back(mk(K_INSERT, 1, 0, 1));
    v.push_back(mk(K_INSERT, PK_IDX_SIZE, 0, 5));
    v.push_back(mk(K_INSERT, PK_IDX_SIZE_P1, 0, 5));   // 守卫拒掉，形态记录在案
    v.push_back(mk(K_INSERT, -1, 0, 5));               // 同上
    v.push_back(mk(K_CLEAR));
    v.push_back(mk(K_VALUE, -2));
    v.push_back(mk(K_VALUE, -1));
    v.push_back(mk(K_VALUE, 0));
    v.push_back(mk(K_VALUE, 1));
    v.push_back(mk(K_VALUE, PK_IDX_SIZE_M1));
    v.push_back(mk(K_VALUE, PK_IDX_SIZE));
    v.push_back(mk(K_VALUE, PK_IDX_SIZE_P1));
    v.push_back(mk(K_VALUE_DEF, -1, 0, 5));
    v.push_back(mk(K_VALUE_DEF, PK_IDX_SIZE, 0, 5));
    v.push_back(mk(K_INDEXOF, 0, 0, 1));
    v.push_back(mk(K_INDEXOF, -1, 0, 1));
    v.push_back(mk(K_INDEXOF, -2, 0, 6));    // token 6 通常不在容器里 → absent
    v.push_back(mk(K_INDEXOF, PK_IDX_SIZE_P1, 0, 1));
    v.push_back(mk(K_LASTINDEXOF, -1, 0, 1));
    v.push_back(mk(K_LASTINDEXOF, 0, 0, 1));
    v.push_back(mk(K_LASTINDEXOF, PK_IDX_SIZE, 0, 1));
    v.push_back(mk(K_LASTINDEXOF, -2, 0, 6));
    v.push_back(mk(K_CONTAINS, 0, 0, 1));
    v.push_back(mk(K_CONTAINS, 0, 0, 6));
    v.push_back(mk(K_COUNT_T, 0, 0, 1));
    v.push_back(mk(K_SIZE));
    v.push_back(mk(K_ISEMPTY));
    v.push_back(mk(K_AT, 0));
    v.push_back(mk(K_AT, PK_IDX_SIZE_M1));
    v.push_back(mk(K_FIRST));
    v.push_back(mk(K_LAST));
    v.push_back(mk(K_ERASE_AT, 0));
    v.push_back(mk(K_ERASE_AT, PK_IDX_SIZE_M1));
    v.push_back(mk(K_RESERVE, 0));
    v.push_back(mk(K_RESERVE, 3));
    v.push_back(mk(K_APPEND_OTHER, 0, 0, 2, 3));
    v.push_back(mk(K_APPEND_SELF));
    v.push_back(mk(K_ASSIGN_SELF));
    v.push_back(mk(K_EQ_OTHER, 0, 0, 0, 1));
}

// 会改状态的指令（depth-3 只用这些）
static bool isMutating(const SeqOp &op)
{
    switch (op.kind) {
    case K_VALUE: case K_VALUE_DEF: case K_INDEXOF: case K_LASTINDEXOF:
    case K_CONTAINS: case K_COUNT_T: case K_SIZE: case K_ISEMPTY:
    case K_AT: case K_FIRST: case K_LAST: case K_RESERVE: case K_EQ_OTHER:
    case K_TOP: case K_HEAD:
        return false;
    default:
        return true;
    }
}

// depth-3 的指令子集：会改状态的，且抽稀到每 2 条取 1（全表三层会到千万量级）
static std::vector<int> mutSubset(const std::vector<SeqOp> &v)
{
    std::vector<int> out;
    for (std::size_t i = 0; i < v.size(); ++i) {
        if (isMutating(v[i]) && (out.size() * 2 <= i)) {
            out.push_back(static_cast<int>(i));
        }
    }
    return out;
}

// ── QStringList 专属：QList<QString> 没有的那几个方法 ─────

static void runStringListExtras()
{
    const std::vector<std::vector<int>> inits = {
        {}, { 1 }, { 1, 1 }, { 2, 1, 3 }, { 0, 1, 2, 3, 4, 5, 6 },
        { 5, 6, 4 },      // U+FFFD / 🎨 / é —— 码元序与码点序结论相反的那一组
        { 6, 5, 6, 5 },
    };
    for (const auto &init : inits) {
        for (int sepTok = 0; sepTok < kNTok; ++sepTok) {
            QStringList q;
            PkStringList p;
            for (int t : init) {
                q.append(PkMake<QString>::make(t));
                p.append(PkMake<PkString>::make(t));
            }
            const std::string ctx = "init=" + dumpSeq(q) + " sep=" + es(PkMake<QString>::make(sepTok));
            const std::string shape = pkEmptyShape(static_cast<int>(init.size()));

            // join
            rec("stringlist.join",
                es(q.join(PkMake<QString>::make(sepTok))) == es(p.join(PkMake<PkString>::make(sepTok))),
                pkTag(shape, false, true), ctx,
                es(q.join(PkMake<QString>::make(sepTok))), es(p.join(PkMake<PkString>::make(sepTok))));

            // filter：needle 取 token
            {
                QStringList qf = q.filter(PkMake<QString>::make(sepTok));
                PkStringList pf = p.filter(PkMake<PkString>::make(sepTok));
                rec("stringlist.filter", dumpSeq(qf) == dumpSeq(pf),
                    pkTag(shape, false, true), ctx, dumpSeq(qf), dumpSeq(pf));
            }

            // sort：两侧的 operator< 口径（码元序 vs 码点序）在这里现形
            {
                QStringList qs = q;
                PkStringList ps = p;
                qs.sort();
                ps.sort();
                rec("stringlist.sort", dumpSeq(qs) == dumpSeq(ps),
                    pkTag(shape, false, true), ctx, dumpSeq(qs), dumpSeq(ps));
            }

            // removeDuplicates：返回删了几个 + 剩下什么
            {
                QStringList qd = q;
                PkStringList pd = p;
                const int qn = qd.removeDuplicates();
                const int pn = pd.removeDuplicates();
                rec("stringlist.removeDuplicates",
                    qn == pn && dumpSeq(qd) == dumpSeq(pd),
                    pkTag(shape, false, true), ctx,
                    es(qn) + " " + dumpSeq(qd), es(pn) + " " + dumpSeq(pd));
            }

            // replaceInStrings
            //
            // tag 的谓词与 .deviation 的理由**逐字对照**（规则二）。理由是
            // 「`before` 为空时替代品**原样返回**，Qt 在每个码元之间插 after」，
            // 所以谓词必须同时要求两件事：
            //   ① before 真的是空串（按**值**判，不是按 token 下标判）
            //   ② 替代品这一侧真的**原样返回**了
            // 少了 ②，将来某个 bug 让空 before 下返回别的东西，也会被这条白名单
            // 罩住 —— 那正是 R-01 `toDouble/failure-value` 踩过的坑。
            for (int aft = 0; aft < kNTok; ++aft) {
                const QString qbefore = PkMake<QString>::make(sepTok);
                QStringList qr = q;
                PkStringList pr = p;
                qr.replaceInStrings(qbefore, PkMake<QString>::make(aft));
                pr.replaceInStrings(PkMake<PkString>::make(sepTok), PkMake<PkString>::make(aft));

                const bool beforeEmpty = qbefore.isEmpty();
                const bool pkUnchanged = (dumpSeq(pr) == dumpSeq(p));
                const std::string rshape =
                    !beforeEmpty ? (shape + "-before-nonempty")
                    : pkUnchanged ? (shape + "-before-empty-pk-unchanged")
                                  : (shape + "-before-empty-pk-changed");

                rec("stringlist.replaceInStrings", dumpSeq(qr) == dumpSeq(pr),
                    pkTag(rshape, false, true),
                    ctx + " after=" + es(PkMake<QString>::make(aft)),
                    dumpSeq(qr), dumpSeq(pr));
            }
        }
    }
}

int main()
{
    oracleBegin("difftest_seq");

    // ---- QVector ↔ PkVector ----
    {
        std::vector<SeqOp> v;
        addCommon(v);
        v.push_back(mk(K_VREMOVE, 0));
        v.push_back(mk(K_VREMOVE, PK_IDX_SIZE_M1));
        v.push_back(mk(K_VREMOVE, 1));
        v.push_back(mk(K_VREMOVE_N, 0, 2));
        v.push_back(mk(K_VREMOVE_N, 1, 0));
        v.push_back(mk(K_RESIZE, 0));
        v.push_back(mk(K_RESIZE, 2));
        v.push_back(mk(K_RESIZE, PK_IDX_SIZE_P1));
        v.push_back(mk(K_FILL, 0, 0, 3));
        v.push_back(mk(K_FILL_N, 2, 0, 3));
        v.push_back(mk(K_FILL_N, 0, 0, 3));
        const std::vector<int> ms = mutSubset(v);
        runSeqSuite<QVector<int>, PkVector<int>, int, int, VecPolicy>("vector", false, v, ms, kInits);
        runSeqSuite<QVector<QString>, PkVector<PkString>, QString, PkString, VecPolicy>("vector", true, v, ms, kInits);
    }

    // ---- QList ↔ PkList / QStringList ↔ PkStringList ----
    {
        std::vector<SeqOp> v;
        addCommon(v);
        v.push_back(mk(K_REMOVEAT, 0));
        v.push_back(mk(K_REMOVEAT, PK_IDX_SIZE_M1));
        v.push_back(mk(K_REMOVEAT, 1));
        v.push_back(mk(K_REMOVEALL, 0, 0, 1));
        v.push_back(mk(K_REMOVEALL, 0, 0, 6));
        v.push_back(mk(K_REMOVEONE, 0, 0, 1));
        v.push_back(mk(K_REMOVEONE, 0, 0, 6));
        v.push_back(mk(K_REMOVEFIRST));
        v.push_back(mk(K_REMOVELAST));
        v.push_back(mk(K_TAKEAT, 0));
        v.push_back(mk(K_TAKEAT, PK_IDX_SIZE_M1));
        v.push_back(mk(K_TAKEFIRST));
        v.push_back(mk(K_TAKELAST));
        v.push_back(mk(K_MOVE, 0, PK_IDX_SIZE_M1));
        v.push_back(mk(K_MOVE, PK_IDX_SIZE_M1, 0));
        v.push_back(mk(K_MOVE, 1, 1));
        const std::vector<int> ms = mutSubset(v);
        runSeqSuite<QList<int>, PkList<int>, int, int, ListPolicy>("list", false, v, ms, kInits);
        runSeqSuite<QList<QString>, PkList<PkString>, QString, PkString, ListPolicy>("list", true, v, ms, kInits);
        runSeqSuite<QStringList, PkStringList, QString, PkString, ListPolicy>("stringlist", true, v, ms, kInits);
    }

    // ---- QStack ↔ PkStack ----
    {
        std::vector<SeqOp> v;
        addCommon(v);
        v.push_back(mk(K_PUSH, 0, 0, 2));
        v.push_back(mk(K_POP));
        v.push_back(mk(K_TOP));
        v.push_back(mk(K_VREMOVE, 0));
        v.push_back(mk(K_RESIZE, 2));
        const std::vector<int> ms = mutSubset(v);
        runSeqSuite<QStack<int>, PkStack<int>, int, int, StackPolicy>("stack", false, v, ms, kInits);
        runSeqSuite<QStack<QString>, PkStack<PkString>, QString, PkString, StackPolicy>("stack", true, v, ms, kInits);
    }

    // ---- QQueue ↔ PkQueue ----
    {
        std::vector<SeqOp> v;
        addCommon(v);
        v.push_back(mk(K_ENQUEUE, 0, 0, 2));
        v.push_back(mk(K_DEQUEUE));
        v.push_back(mk(K_HEAD));
        v.push_back(mk(K_REMOVEAT, 0));
        v.push_back(mk(K_TAKELAST));
        const std::vector<int> ms = mutSubset(v);
        runSeqSuite<QQueue<int>, PkQueue<int>, int, int, QueuePolicy>("queue", false, v, ms, kInits);
        runSeqSuite<QQueue<QString>, PkQueue<PkString>, QString, PkString, QueuePolicy>("queue", true, v, ms, kInits);
    }

    runStringListExtras();

    return oracleEnd();
}
