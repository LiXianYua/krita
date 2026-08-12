// difftest_assoc.cpp —— 关联容器族与真 Qt 5 对拍
//
//   QMap<K,V>  ↔ PkMap<K,V>
//   QHash<K,V> ↔ PkHash<K,V>
//
// 输入模型同 difftest_seq.cpp：一次用例 =（key/value 类型, 初始键值对, 操作序列,
// 共享与否），逐条操作比「返回值 + 自身 dump + alias dump」。
//
// ── 有序与无序分两条通道 ──────────────────────────────────
// **QHash 的迭代顺序不可对拍**：Qt5 的 QHash 带随机化种子，`std::unordered_map`
// 又是另一套。实测同一段代码 Qt 5.15.7 打出 `6,7,4,5,2,3,0,1`，5.15.13 打出
// `4,5,6,7,0,1,2,3` —— **跨 Qt 补丁版本就变**，拿它当基准是错的。
// 所以 hash 通道只比**排序后的集合**，「顺序」这一维完全没有覆盖。
//
// QMap 相反：`QMap` 与 `std::map` 都按 key 的 `operator<` 排序，迭代顺序是
// **可观察且该一致的**。map 通道因此直接按迭代顺序比 —— 这条正好把
// 「`PkString::operator<` 与 `QString::operator<` 是不是同一个口径」压出来。
//
// ── key(value) 的守卫 ─────────────────────────────────────
// `key(v)` 返回「某个映射到 v 的 key」。有多个 key 映射到同一个 v 时，选哪个
// 由迭代顺序决定 —— hash 通道下不可对拍。所以 hash 通道只在「至多一个 key 映射
// 到 v」时才生成这条比对（形态 `value-uniq`）；map 通道无此限制。

// ── 一个只在对拍里出现的冲突，以及它为什么必须这样解 ────
// `PkHashFunctions.h` 在**全局命名空间**给内建类型定义了一整组 `qHash` 重载
// （`qHash(char)`/`qHash(short)`/…），签名与真 Qt 的 `qhashfunctions.h` 逐字相同。
// 这是**设计要求**：Krita 里那 18 处自定义 `uint qHash(const KoID &)` 靠的正是
// 这个名字与这套 ADL 规则，替代品必须原样承接。代价是**真 Qt 与替代品的这组
// 重载没法出现在同一个翻译单元里**（重复定义，编译期报错）。
//
// 解法：把替代品那一侧的 `qHash` 在预处理层改名。宏在**扫描到头文件正文时**展开，
// 所以先 include 的 Qt 头文件里那些 `qHash` token 早已定型、不受影响；替代品这边
// 定义处（PkHashFunctions.h / PkStringHash.h）与调用处（PkHasher 里的
// `qHash(key)`）同时被改名，ADL 仍然命中，哈希值本身在 Qt 里也不可观察
// （QHash 迭代顺序未定义），所以被测行为一点没变。
//
// 改名有没有误伤 Qt 那一侧，下面有一条 `static_assert` 当场验，不靠推断。
#include <QMap>
#include <QHash>
#include <QList>
#include <QString>

#define qHash pkOracleHash
#include <PkMap.h>
#include <PkHash.h>
#include <PkStringHash.h>   // qHash(const PkString &) —— PkHash<PkString,V> 要它
#undef qHash

#include "difftest_common.h"

// 真 Qt 的 `uint qHash(const QString &, uint)` 必须原样还在 —— 证明上面那个宏
// 只改了替代品一侧。
static_assert(std::is_same<decltype(qHash(QString())), uint>::value,
              "qHash 改名误伤了真 Qt 那一侧");

#include <string>
#include <type_traits>
#include <vector>

static_assert(!std::is_same<QMap<int, int>, PkMap<int, int>>::value, "QMap/PkMap 编成了同一类型");
static_assert(!std::is_same<QHash<int, int>, PkHash<int, int>>::value, "QHash/PkHash 编成了同一类型");

// ── 指令集 ────────────────────────────────────────────────

enum AssocKind {
    A_INSERT = 0, A_REMOVE, A_TAKE, A_VALUE, A_VALUE_DEF, A_CONTAINS,
    A_COUNT_K, A_SIZE, A_ISEMPTY, A_KEYS, A_KEYS_V, A_VALUES, A_VALUES_K,
    A_KEY_V, A_KEY_V_DEF, A_BRACKET_READ, A_BRACKET_WRITE, A_CLEAR,
    A_FIND, A_CONSTFIND, A_ERASE_FIND, A_ASSIGN_SELF, A_EQ_OTHER,
};

struct AssocOp {
    int kind;
    int k;    // key token 下标
    int v;    // value token 下标
    int k2;   // 第二个 key token（构造 other 用）
};

static AssocOp mk(int kind, int k = 0, int v = 0, int k2 = 1)
{
    AssocOp op; op.kind = kind; op.k = k; op.v = v; op.k2 = k2; return op;
}

struct AssocPlan {
    bool run = true;
    const char *api = "?";
    std::string shape;
};

// ── 规划 ──────────────────────────────────────────────────
//
// 形态由「这个 key 在不在容器里 / 容器空不空 / 这个 value 有几个 key 映射到它」
// 算出来，不是由 API 名字算出来的（规则一）。

static AssocPlan planAssoc(const AssocOp &op, int n, bool keyPresent,
                           long valueOwners, bool ordered)
{
    AssocPlan pl;
    switch (op.kind) {
    case A_INSERT:
        pl.api = "insert";
        // 「插入已存在的 key」= 覆盖语义，与「插入新 key」是两个形态
        pl.shape = keyPresent ? "dup-key" : "uniq-key";
        break;
    case A_REMOVE:     pl.api = "remove";     pl.shape = pkPresentShape(keyPresent); break;
    case A_TAKE:       pl.api = "take";       pl.shape = pkPresentShape(keyPresent); break;
    case A_VALUE:      pl.api = "value";      pl.shape = pkPresentShape(keyPresent); break;
    case A_VALUE_DEF:  pl.api = "value-def";  pl.shape = pkPresentShape(keyPresent); break;
    case A_CONTAINS:   pl.api = "contains";   pl.shape = pkPresentShape(keyPresent); break;
    case A_COUNT_K:    pl.api = "count-k";    pl.shape = pkPresentShape(keyPresent); break;
    case A_VALUES_K:   pl.api = "values-k";   pl.shape = pkPresentShape(keyPresent); break;
    case A_FIND:       pl.api = "find";       pl.shape = pkPresentShape(keyPresent); break;
    case A_CONSTFIND:  pl.api = "constFind";  pl.shape = pkPresentShape(keyPresent); break;
    case A_BRACKET_READ:  pl.api = "bracket-read";  pl.shape = pkPresentShape(keyPresent); break;
    case A_BRACKET_WRITE: pl.api = "bracket-write"; pl.shape = pkPresentShape(keyPresent); break;

    case A_ERASE_FIND:
        pl.api = "erase-find";
        pl.shape = pkPresentShape(keyPresent);
        // erase(constEnd()) 是已知的两侧不同处，且 Qt 那边是未定义的格子 —— 不生成
        pl.run = keyPresent;
        break;

    case A_SIZE:    pl.api = "size";    pl.shape = pkEmptyShape(n); break;
    case A_ISEMPTY: pl.api = "isEmpty"; pl.shape = pkEmptyShape(n); break;
    case A_CLEAR:   pl.api = "clear";   pl.shape = pkEmptyShape(n); break;
    case A_KEYS:    pl.api = "keys";    pl.shape = pkEmptyShape(n); break;
    case A_VALUES:  pl.api = "values";  pl.shape = pkEmptyShape(n); break;
    case A_ASSIGN_SELF: pl.api = "assign-self"; pl.shape = pkEmptyShape(n); break;
    case A_EQ_OTHER:    pl.api = "operator==";  pl.shape = pkEmptyShape(n); break;

    case A_KEYS_V:
        pl.api = "keys-v";
        pl.shape = valueOwners == 0 ? "value-absent"
                 : (valueOwners == 1 ? "value-uniq" : "value-dup");
        break;
    case A_KEY_V:
    case A_KEY_V_DEF:
        pl.api = (op.kind == A_KEY_V) ? "key-v" : "key-v-def";
        pl.shape = valueOwners == 0 ? "value-absent"
                 : (valueOwners == 1 ? "value-uniq" : "value-dup");
        // 无序通道下「多个 key 映射到同一个 value」时选哪个 key 由迭代顺序定，
        // 那是不可对拍的格子。**谓词与理由等宽：只在 valueOwners>1 且无序时拒。**
        pl.run = ordered || valueOwners <= 1;
        break;

    default: pl.run = false; break;
    }
    return pl;
}

// ── 执行 ──────────────────────────────────────────────────
//
// 同一份模板分别用 `QMap<QString,int>` 与 `PkMap<PkString,int>` 实例化：
// 编得过本身就是「方法名与签名跟 Qt 一致」的证明。

template <typename M, typename K, typename V>
static M mkOther(const AssocOp &op)
{
    M m;
    m.insert(PkMake<K>::make(op.k), PkMake<V>::make(op.v));
    m.insert(PkMake<K>::make(op.k2), PkMake<V>::make(op.v));
    return m;
}

template <typename M, typename K, typename V>
static std::string execAssoc(M &m, const AssocOp &op, bool ordered)
{
    const K k = PkMake<K>::make(op.k);
    const V v = PkMake<V>::make(op.v);
    switch (op.kind) {
    case A_INSERT:     m.insert(k, v); return "-";
    case A_REMOVE:     return es(m.remove(k));
    case A_TAKE:       return es(m.take(k));
    case A_VALUE:      return es(m.value(k));
    case A_VALUE_DEF:  return es(m.value(k, v));
    case A_CONTAINS:   return es(m.contains(k));
    case A_COUNT_K:    return es(m.count(k));
    case A_SIZE:       return es(m.size());
    case A_ISEMPTY:    return es(m.isEmpty());
    case A_CLEAR:      m.clear(); return "-";
    case A_KEYS:       return ordered ? dumpSeq(m.keys())      : dumpListSorted(m.keys());
    case A_VALUES:     return ordered ? dumpSeq(m.values())    : dumpListSorted(m.values());
    case A_KEYS_V:     return ordered ? dumpSeq(m.keys(v))     : dumpListSorted(m.keys(v));
    case A_VALUES_K:   return ordered ? dumpSeq(m.values(k))   : dumpListSorted(m.values(k));
    case A_KEY_V:      return es(m.key(v));
    case A_KEY_V_DEF:  return es(m.key(v, k));
    case A_BRACKET_READ:  return es(m[k]);            // 非 const operator[]：key 缺失时插入默认值
    case A_BRACKET_WRITE: m[k] = v; return "-";
    case A_ASSIGN_SELF: { const M *self = &m; m = *self; return "-"; }
    case A_EQ_OTHER:    { const M other = mkOther<M, K, V>(op); return es(m == other); }
    case A_FIND: {
        auto it = m.find(k);
        return it == m.end() ? std::string("end") : es(it.value());
    }
    case A_CONSTFIND: {
        auto it = m.constFind(k);
        return it == m.constEnd() ? std::string("end") : es(it.value());
    }
    case A_ERASE_FIND: {
        auto it = m.erase(m.find(k));
        // **无序通道下这个返回值一格都不可对拍。** `erase(it)` 返回「迭代顺序里的
        // 下一个」，而 QHash 的迭代顺序未定义 —— 连「是不是恰好到头了」都跟着顺序
        // 走。实测：两元素的 hash 里删一个，QHash 说 end、unordered_map 说 not-end，
        // 反之亦然（`{0}→bracket-read→insert→erase-find` 一组 6646 次）。那是 Qt
        // 没有定义的格子，记成差异是**假差异**，为它写一条 deviation 更是错的。
        // 所以无序通道只比 erase 之后的**状态**（外层已经在比），返回值统一成常量。
        if (!ordered) {
            return "-";
        }
        if (it == m.end()) {
            return "end";
        }
        return es(it.key()) + "=" + es(it.value());
    }
    default: break;
    }
    return "??";
}

// ── 用例执行 ──────────────────────────────────────────────

template <typename QM, typename PM, typename QK, typename PK_, typename V>
static void runAssocSuite(const char *suite, bool ordered, bool keyStr,
                          const std::vector<AssocOp> &instrs,
                          const std::vector<int> &mutIdx,
                          const std::vector<std::vector<int>> &inits)
{
    auto dq = [&](const QM &m) { return ordered ? dumpAssocOrdered(m) : dumpAssocSorted(m); };
    auto dp = [&](const PM &m) { return ordered ? dumpAssocOrdered(m) : dumpAssocSorted(m); };

    pkForEachCase(static_cast<int>(instrs.size()), mutIdx, inits,
        [&](const std::vector<int> &seq, const std::vector<int> &init, bool shared) {
            QM q;
            PM p;
            // init 里每个元素是 key token 下标；value 用 (下标+1)%kNTok 派生，
            // 这样「同一个 value 被多个 key 映射」与「value 唯一」两种形态都出得来。
            for (int t : init) {
                q.insert(PkMake<QK>::make(t), PkMake<V>::make((t + 1) % 3));
                p.insert(PkMake<PK_>::make(t), PkMake<V>::make((t + 1) % 3));
            }
            QM qa;
            PM pa;
            if (shared) { qa = q; pa = p; }

            std::string ctx = "init=" + dq(q);

            for (std::size_t s = 0; s < seq.size(); ++s) {
                const AssocOp &op = instrs[seq[s]];
                const QK qk = PkMake<QK>::make(op.k);
                const V qv = PkMake<V>::make(op.v);

                const bool keyPresent = q.contains(qk);
                long valueOwners = 0;
                for (auto it = q.constBegin(); it != q.constEnd(); ++it) {
                    if (it.value() == qv) { ++valueOwners; }
                }

                const AssocPlan pl = planAssoc(op, q.size(), keyPresent, valueOwners, ordered);
                if (!pl.run) {
                    continue;
                }

                const std::string qret = execAssoc<QM, QK, V>(q, op, ordered);
                const std::string pret = execAssoc<PM, PK_, V>(p, op, ordered);
                const std::string qd = dq(q);
                const std::string pd = dp(p);
                std::string qad, pad;
                if (shared) { qad = dq(qa); pad = dp(pa); }

                const bool same = (qret == pret) && (qd == pd) && (qad == pad);
                rec(std::string(suite) + "." + pl.api, same,
                    pkTag(pl.shape, shared, keyStr),
                    ctx + " ; " + pl.api + "(k=" + es(qk) + ",v=" + es(qv) + ")",
                    qret + " " + qd + (shared ? " alias=" + qad : std::string()),
                    pret + " " + pd + (shared ? " alias=" + pad : std::string()));

                if (qd != pd || qad != pad) {
                    break;   // 状态岔开，后面的 tag 会张冠李戴
                }
                ctx += " ; " + std::string(pl.api);
            }
        });
}

// ── 初始状态与指令表 ──────────────────────────────────────

static const std::vector<std::vector<int>> kInits = {
    {},                     // 空
    { 1 },                  // 单键
    { 1, 2 },
    { 0, 1, 2, 3 },
    { 0, 1, 2, 3, 4, 5, 6 },    // 全部 token（字符串通道下含 é / U+FFFD / 🎨）
    { 4, 5, 6 },                // 只有三个非 ASCII —— 压 operator< 的排序口径
};

static std::vector<AssocOp> buildInstrs()
{
    std::vector<AssocOp> v;
    for (int k : { 0, 1, 5, 6 }) {          // 有的在初始集里、有的不在 → 两种形态都出
        v.push_back(mk(A_INSERT, k, 0));
        v.push_back(mk(A_INSERT, k, 2));
        v.push_back(mk(A_REMOVE, k));
        v.push_back(mk(A_TAKE, k));
        v.push_back(mk(A_VALUE, k));
        v.push_back(mk(A_VALUE_DEF, k, 5));
        v.push_back(mk(A_CONTAINS, k));
        v.push_back(mk(A_COUNT_K, k));
        v.push_back(mk(A_VALUES_K, k));
        v.push_back(mk(A_FIND, k));
        v.push_back(mk(A_CONSTFIND, k));
        v.push_back(mk(A_ERASE_FIND, k));
        v.push_back(mk(A_BRACKET_READ, k));
        v.push_back(mk(A_BRACKET_WRITE, k, 3));
    }
    for (int val : { 0, 1, 2, 5 }) {
        v.push_back(mk(A_KEYS_V, 0, val));
        v.push_back(mk(A_KEY_V, 0, val));
        v.push_back(mk(A_KEY_V_DEF, 6, val));
    }
    v.push_back(mk(A_SIZE));
    v.push_back(mk(A_ISEMPTY));
    v.push_back(mk(A_CLEAR));
    v.push_back(mk(A_KEYS));
    v.push_back(mk(A_VALUES));
    v.push_back(mk(A_ASSIGN_SELF));
    v.push_back(mk(A_EQ_OTHER, 0, 1, 1));
    return v;
}

static bool isMutating(const AssocOp &op)
{
    switch (op.kind) {
    case A_VALUE: case A_VALUE_DEF: case A_CONTAINS: case A_COUNT_K:
    case A_SIZE: case A_ISEMPTY: case A_KEYS: case A_KEYS_V:
    case A_VALUES: case A_VALUES_K: case A_KEY_V: case A_KEY_V_DEF:
    case A_FIND: case A_CONSTFIND: case A_EQ_OTHER:
        return false;
    default:
        return true;
    }
}

// depth-3 的子集：只用会改状态的指令，且抽稀（全表三层会到千万量级）
static std::vector<int> mutSubset(const std::vector<AssocOp> &v)
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
    oracleBegin("difftest_assoc");

    const std::vector<AssocOp> instrs = buildInstrs();
    const std::vector<int> ms = mutSubset(instrs);

    runAssocSuite<QMap<int, int>, PkMap<int, int>, int, int, int>(
        "map", true, false, instrs, ms, kInits);
    runAssocSuite<QMap<QString, int>, PkMap<PkString, int>, QString, PkString, int>(
        "map", true, true, instrs, ms, kInits);
    runAssocSuite<QHash<int, int>, PkHash<int, int>, int, int, int>(
        "hash", false, false, instrs, ms, kInits);
    runAssocSuite<QHash<QString, int>, PkHash<PkString, int>, QString, PkString, int>(
        "hash", false, true, instrs, ms, kInits);

    return oracleEnd();
}
