#include "PkJavaIteratorTest.h"

#include "../PkHashIterator.h"
#include "../PkListIterator.h"
#include "../PkMapIterator.h"
#include "../PkVectorIterator.h"

#include <cstdio>
#include <type_traits>

#include "pk_binder_PkJavaIteratorTest.inc"

namespace {

// 输出改行缓冲：断言失败若伴随段错误，全缓冲的 stdout 会吞掉崩溃前的行。
struct PkLineBufferedStdout
{
    PkLineBufferedStdout() { std::setvbuf(stdout, nullptr, _IOLBF, 0); }
};
const PkLineBufferedStdout g_pkLineBuffered;

// 带拷贝计数器的元素类型（同 PkSeqTestShared.h 的 PkSeqCounted，理由见那里）。
struct PkJavaCounted
{
    int v = 0;

    PkJavaCounted() = default;
    explicit PkJavaCounted(int x) : v(x) {}
    PkJavaCounted(const PkJavaCounted &o) : v(o.v) { ++s_copies; }
    PkJavaCounted &operator=(const PkJavaCounted &o)
    {
        v = o.v;
        ++s_copies;
        return *this;
    }
    PkJavaCounted(PkJavaCounted &&) noexcept = default;
    PkJavaCounted &operator=(PkJavaCounted &&) noexcept = default;
    ~PkJavaCounted() = default;

    bool operator==(const PkJavaCounted &o) const { return v == o.v; }

    static int s_copies;
};

int PkJavaCounted::s_copies = 0;

// libs/command/kundo2stack.cpp:403 的形态：实参是函数返回的**临时容器**。
PkVector<int> pkMakeVector()
{
    PkVector<int> v;
    v.append(1);
    v.append(2);
    v.append(3);
    return v;
}

// "源容器已经析构，迭代器照样有效" —— 只有真的持拷贝才成立。
PkListIterator<int> pkIteratorOverLocalList()
{
    PkList<int> local{5, 6, 7};
    return PkListIterator<int>(local);
}

PkMapIterator<int, int> pkIteratorOverLocalMap()
{
    PkMap<int, int> local;
    local.insert(1, 10);
    local.insert(2, 20);
    return PkMapIterator<int, int>(local);
}

// libs/widgetutils/xmlgui/kxmlguifactory_p.cpp:75 的形态：可变迭代器按引用传参，
// 被调方从中删一个元素。
void pkRemoveOneThrough(PkMutableListIterator<int> &it)
{
    if (it.hasNext()) {
        it.next();
        it.remove();
    }
}

} // namespace

// ---------------------------------------------------------------------------
// 只读序列迭代器
// ---------------------------------------------------------------------------

void PkJavaIteratorTest::listIteratorForwardTraversal()
{
    PkList<int> l{1, 2, 3};
    PkListIterator<int> it(l);

    PK_VERIFY(it.hasNext());
    PK_COMPARE(it.peekNext(), 1);
    // peekNext 不移动游标
    PK_COMPARE(it.peekNext(), 1);
    PK_COMPARE(it.next(), 1);
    PK_COMPARE(it.next(), 2);
    PK_COMPARE(it.peekNext(), 3);
    PK_COMPARE(it.next(), 3);
    PK_VERIFY(!it.hasNext());

    // 走到底之后 hasPrevious 为真（游标在最后一个元素之后）
    PK_VERIFY(it.hasPrevious());
    PK_COMPARE(it.previous(), 3);

    // 空容器：一进来就没有 next，也没有 previous
    PkList<int> empty;
    PkListIterator<int> emptyIt(empty);
    PK_VERIFY(!emptyIt.hasNext());
    PK_VERIFY(!emptyIt.hasPrevious());

    // 典型 while 形态（保留范围内 6 处 hasNext + 6 处 next 全长这样）
    PkList<int> sumSrc{10, 20, 30};
    PkListIterator<int> sumIt(sumSrc);
    int sum = 0;
    while (sumIt.hasNext()) {
        sum += sumIt.next();
    }
    PK_COMPARE(sum, 60);
}

void PkJavaIteratorTest::listIteratorBackwardTraversal()
{
    // libs/widgetutils/xmlgui/kedittoolbar.cpp:961 的形态：
    //   clientIterator.toBack(); while (clientIterator.hasPrevious()) { ... previous() ... }
    PkList<int> l{1, 2, 3};
    PkListIterator<int> it(l);

    it.toBack();
    PK_VERIFY(!it.hasNext());
    PK_VERIFY(it.hasPrevious());
    PK_COMPARE(it.peekPrevious(), 3);
    PK_COMPARE(it.peekPrevious(), 3);   // 不移动游标
    PK_COMPARE(it.previous(), 3);
    PK_COMPARE(it.previous(), 2);
    PK_COMPARE(it.previous(), 1);
    PK_VERIFY(!it.hasPrevious());

    // toFront 回到开头
    it.toFront();
    PK_VERIFY(it.hasNext());
    PK_VERIFY(!it.hasPrevious());
    PK_COMPARE(it.next(), 1);

    // libs/widgets/KoRuler.cpp:661 的形态：peekPrevious 与 peekNext 同时用
    //   while (i.hasNext() && i.hasPrevious()) drawDistanceLine(i.peekPrevious(), i.peekNext());
    PkList<int> ruler{10, 20, 30};
    PkListIterator<int> rulerIt(ruler);
    rulerIt.next();   // 游标落在 10 与 20 之间
    PK_VERIFY(rulerIt.hasNext() && rulerIt.hasPrevious());
    PK_COMPARE(rulerIt.peekPrevious(), 10);
    PK_COMPARE(rulerIt.peekNext(), 20);
}

void PkJavaIteratorTest::vectorIteratorTraversal()
{
    // libs/command/kundo2stack.cpp:403/417 的两种形态各来一遍
    PkVector<int> v{1, 2, 3};

    // toFront（保留范围内唯一一处：kundo2stack.cpp:404）
    PkVectorIterator<int> forward(v);
    forward.next();
    forward.toFront();
    PK_COMPARE(forward.next(), 1);

    // toBack + hasPrevious + previous
    PkVectorIterator<int> backward(v);
    backward.toBack();
    int sum = 0;
    while (backward.hasPrevious()) {
        sum += backward.previous();
    }
    PK_COMPARE(sum, 6);

    // peekPrevious（libs/resources/KisMemoryStorage.cpp:73 的形态：
    //   next() 之后用 peekPrevious() 拿"刚刚返回的那个"）
    PkVectorIterator<int> peek(v);
    peek.next();
    PK_COMPARE(peek.peekPrevious(), 1);
    peek.next();
    PK_COMPARE(peek.peekPrevious(), 2);

    // PkListIterator<T> 与 PkVectorIterator<T> 是**不同类型**（内层容器不同）
    static_assert(!std::is_same<PkVectorIterator<int>, PkListIterator<int>>::value,
                  "PkVectorIterator 与 PkListIterator 必须是不同类型");
}

void PkJavaIteratorTest::readOnlyIteratorHoldsCopy()
{
    PkList<PkJavaCounted> l;
    l.reserve(8);
    for (int k = 1; k <= 3; ++k) {
        l.append(PkJavaCounted(k));
    }
    PK_COMPARE(l.PkUseCount(), 1L);

    PkJavaCounted::s_copies = 0;
    PkListIterator<PkJavaCounted> it(l);
    const int copiesAfterConstruction = PkJavaCounted::s_copies;

    // 构造迭代器 = 一次 O(1) 的容器拷贝：引用计数 2、元素一个都没拷。
    // 这两条缺一不可 —— 只看引用计数漏不掉"多拷了元素"，只看拷贝计数漏不掉
    // "干脆没拷、直接引用了原容器"。
    PK_COMPARE(copiesAfterConstruction, 0);
    PK_COMPARE(l.PkUseCount(), 2L);

    // 构造之后修改原容器：迭代器那一份是独立的快照
    l.append(PkJavaCounted(4));
    l.removeAt(0);
    PK_COMPARE(l.size(), 3);
    PK_COMPARE(l.PkUseCount(), 1L);   // 原容器 detach 走了

    int seen = 0;
    int total = 0;
    while (it.hasNext()) {
        total += it.next().v;
        ++seen;
    }
    PK_COMPARE(seen, 3);
    PK_COMPARE(total, 6);   // 仍是构造时的 1+2+3，不是改完之后的 2+3+4

    // 关联侧同理
    PkMap<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    PkMapIterator<int, int> mapIt(m);
    PK_COMPARE(m.PkUseCount(), 2L);
    m.insert(3, 30);
    PK_COMPARE(m.size(), 3);
    int mapRounds = 0;
    while (mapIt.hasNext()) {
        mapIt.next();
        ++mapRounds;
    }
    PK_COMPARE(mapRounds, 2);
}

void PkJavaIteratorTest::readOnlyIteratorOutlivesSourceContainer()
{
    // ① 实参是临时容器（kundo2stack.cpp:403 的 mergeCommandsVector()）
    PkVectorIterator<int> tempIt(pkMakeVector());
    int sum = 0;
    while (tempIt.hasNext()) {
        sum += tempIt.next();
    }
    PK_COMPARE(sum, 6);

    // ② 源容器是已经析构的局部量 —— 只有真的持拷贝才不悬垂
    PkListIterator<int> localIt = pkIteratorOverLocalList();
    int localSum = 0;
    while (localIt.hasNext()) {
        localSum += localIt.next();
    }
    PK_COMPARE(localSum, 18);

    // ③ 关联侧同样（KoProperties::propertyIterator 按值返回的正是这个形状）
    PkMapIterator<int, int> mapIt = pkIteratorOverLocalMap();
    int mapSum = 0;
    int keySum = 0;
    while (mapIt.hasNext()) {
        mapIt.next();
        keySum += mapIt.key();
        mapSum += mapIt.value();
    }
    PK_COMPARE(keySum, 3);
    PK_COMPARE(mapSum, 30);
}

// ---------------------------------------------------------------------------
// 可变序列迭代器
// ---------------------------------------------------------------------------

void PkJavaIteratorTest::mutableListIteratorTraversal()
{
    PkList<int> l{1, 2, 3};
    PkMutableListIterator<int> it(l);

    PK_VERIFY(it.hasNext());
    PK_COMPARE(it.peekNext(), 1);
    PK_COMPARE(it.next(), 1);
    PK_COMPARE(it.next(), 2);
    PK_COMPARE(it.peekNext(), 3);
    PK_COMPARE(it.next(), 3);
    PK_VERIFY(!it.hasNext());

    // next() 返回 **T&**（Qt 同样）：kxmlguifactory_p.cpp:78
    // `delete childIterator.next();` 与 :334 `cmIt.next().clientName` 靠这条。
    // 写进去就是写原容器。
    PkList<int> w{1, 2};
    PkMutableListIterator<int> writer(w);
    writer.next() = 100;
    writer.next() = 200;
    PK_COMPARE(w.at(0), 100);
    PK_COMPARE(w.at(1), 200);

    static_assert(std::is_same<decltype(writer.next()), int &>::value,
                  "PkMutableListIterator::next() 必须返回 T&");
    static_assert(std::is_same<decltype(writer.peekNext()), int &>::value,
                  "PkMutableListIterator::peekNext() 必须返回 T&");

    // toBack + hasPrevious + previous（kis_simple_update_queue.cpp:251 的形态）
    PkList<int> b{1, 2, 3};
    PkMutableListIterator<int> back(b);
    back.toBack();
    PK_VERIFY(!back.hasNext());
    int sum = 0;
    while (back.hasPrevious()) {
        sum += back.previous();
    }
    PK_COMPARE(sum, 6);

    // 空容器
    PkList<int> empty;
    PkMutableListIterator<int> emptyIt(empty);
    PK_VERIFY(!emptyIt.hasNext());
    PK_VERIFY(!emptyIt.hasPrevious());
}

void PkJavaIteratorTest::mutableListIteratorRemove()
{
    // ① 前向删（kxmlguifactory_p.cpp:332 的形态：next() 之后条件成立就 remove()）
    PkList<int> forward{1, 2, 3, 4, 5};
    PkMutableListIterator<int> fwd(forward);
    while (fwd.hasNext()) {
        if (fwd.next() % 2 == 0) {
            fwd.remove();
        }
    }
    PK_VERIFY((forward == PkList<int>{1, 3, 5}));

    // ② 反向删（kis_simple_update_queue.cpp:253 / kis_tile_data_pooler.cc:341 的形态）
    PkList<int> backward{1, 2, 3, 4, 5};
    PkMutableListIterator<int> bwd(backward);
    bwd.toBack();
    while (bwd.hasPrevious()) {
        if (bwd.previous() % 2 == 0) {
            bwd.remove();
        }
    }
    PK_VERIFY((backward == PkList<int>{1, 3, 5}));

    // ③ 全删光
    PkList<int> all{1, 2, 3};
    PkMutableListIterator<int> allIt(all);
    while (allIt.hasNext()) {
        allIt.next();
        allIt.remove();
    }
    PK_COMPARE(all.size(), 0);

    // ④ 没有"上一次返回的元素"时 remove() 是 no-op（Qt 同样）
    PkList<int> noop{1, 2};
    PkMutableListIterator<int> noopIt(noop);
    noopIt.remove();                 // 还没 next() 过
    PK_COMPARE(noop.size(), 2);
    noopIt.next();
    noopIt.remove();
    noopIt.remove();                 // 连着删第二次：上一次的标记已作废
    PK_COMPARE(noop.size(), 1);
    PK_COMPARE(noop.at(0), 2);

    // ⑤ 删完之后游标位置正确：接着 next() 拿到的是被删元素的后一个
    PkList<int> cursor{1, 2, 3, 4};
    PkMutableListIterator<int> cursorIt(cursor);
    cursorIt.next();          // 1
    cursorIt.next();          // 2
    cursorIt.remove();        // 删 2
    PK_COMPARE(cursorIt.next(), 3);
    PK_VERIFY((cursor == PkList<int>{1, 3, 4}));
}

void PkJavaIteratorTest::mutableListIteratorReallyModifiesOriginal()
{
    // 可变迭代器持**指针**：改的是原容器本体。与只读版的"持拷贝"正好相反，
    // 这一对正反面是 Qt 这套设计的核心，两条都要压。
    PkList<int> l{1, 2, 3};
    PkList<int> shadow(l);
    PK_VERIFY(l.PkIsSharedWith(shadow));

    PkMutableListIterator<int> it(l);
    it.next();
    it.remove();

    // 原容器真的变了
    PK_VERIFY((l == PkList<int>{2, 3}));
    // 共享的另一份一个字节都没变（写经 PkMut() → 正确 detach）
    PK_VERIFY((shadow == PkList<int>{1, 2, 3}));
    PK_VERIFY(!l.PkIsSharedWith(shadow));

    // 写值同样：next() 返回的引用写进去要经 PkMut()
    PkList<int> w{7, 8};
    PkList<int> wShadow(w);
    PkMutableListIterator<int> writer(w);
    writer.next() = 70;
    PK_COMPARE(w.at(0), 70);
    PK_COMPARE(wShadow.at(0), 7);
    PK_VERIFY(!w.PkIsSharedWith(wShadow));
}

void PkJavaIteratorTest::mutableListIteratorConstructionShapes()
{
    // ① 从**非 const** 容器拷贝初始化（kxmlguifactory_p.cpp:332/366）：
    //    `QMutableListIterator<MergingIndex> cmIt = mergingIndices;`
    //    构造函数不能是 explicit，否则这一行当场编不过。
    PkList<int> l{1, 2, 3};
    PkMutableListIterator<int> it = l;
    PK_COMPARE(it.next(), 1);

    // ② 按引用传参（kxmlguifactory_p.cpp:75 的 removeChild）
    PkList<int> byRef{1, 2, 3};
    PkMutableListIterator<int> refIt(byRef);
    pkRemoveOneThrough(refIt);
    PK_VERIFY((byRef == PkList<int>{2, 3}));
    // 传参前后是同一个迭代器，游标接着走
    PK_COMPARE(refIt.next(), 2);

    // ③ 只读版也要能拷贝初始化（kedittoolbar.cpp:960）
    PkList<int> ro{4, 5};
    PkListIterator<int> roIt = ro;
    PK_COMPARE(roIt.next(), 4);
}

// ---------------------------------------------------------------------------
// 关联迭代器
// ---------------------------------------------------------------------------

void PkJavaIteratorTest::mapIteratorKeyValue()
{
    // 42 处 key()/value() 调用点全长这个样子：
    //   while (it.hasNext()) { it.next(); use(it.key(), it.value()); }
    // key()/value() 读的是**上一次 next() 返回的那一项**，不是"下一个"。
    PkMap<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    PkMapIterator<int, int> it(m);
    PkList<int> keys;
    PkList<int> values;
    while (it.hasNext()) {
        it.next();
        keys.append(it.key());
        values.append(it.value());
    }
    // PkMap 内层是 std::map，迭代顺序按 key 升序（QMap 同样）
    PK_VERIFY((keys == PkList<int>{1, 2, 3}));
    PK_VERIFY((values == PkList<int>{10, 20, 30}));

    // key()/value() 不移动游标：连着读两次是同一项
    PkMapIterator<int, int> stable(m);
    stable.next();
    PK_COMPARE(stable.key(), 1);
    PK_COMPARE(stable.key(), 1);
    PK_COMPARE(stable.value(), 10);
    PK_COMPARE(stable.value(), 10);
    stable.next();
    PK_COMPARE(stable.key(), 2);

    // 空容器
    PkMap<int, int> empty;
    PkMapIterator<int, int> emptyIt(empty);
    PK_VERIFY(!emptyIt.hasNext());

    // 构造迭代器不 detach（走的是 constBegin/constEnd）：原来共享的两份仍然共享
    PkMap<int, int> shared;
    shared.insert(1, 1);
    PkMap<int, int> other(shared);
    PkMapIterator<int, int> sharedIt(shared);
    PK_VERIFY(shared.PkIsSharedWith(other));
    // 三方共享同一块缓冲区
    PK_COMPARE(shared.PkUseCount(), 3L);
    sharedIt.next();
    PK_COMPARE(sharedIt.key(), 1);
    PK_VERIFY(shared.PkIsSharedWith(other));
}

void PkJavaIteratorTest::mapIteratorCopyAndAssign()
{
    PkMap<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 30);

    // ① 拷贝构造：游标位置跟着一起拷（libs/image/kis_base_node.cpp:58
    //    `QMapIterator<...> iter = rhs.properties.propertyIterator();` 的内核）
    PkMapIterator<int, int> a(m);
    a.next();
    a.next();
    PK_COMPARE(a.key(), 2);

    PkMapIterator<int, int> b(a);
    PK_COMPARE(b.key(), 2);   // 上一次返回的那一项也拷过来了
    // 两边的"下一个"是同一项，且各走各的不互相影响
    b.next();
    PK_COMPARE(b.key(), 3);
    PK_COMPARE(a.key(), 2);
    a.next();
    PK_COMPARE(a.key(), 3);
    PK_VERIFY(!b.hasNext());
    PK_VERIFY(!a.hasNext());

    // ② 拷贝赋值（plugins/impex/libkra/kis_kra_savexml_visitor.cpp:186
    //    `i = QMapIterator<const KisNode*, QString>(visitor.keyframeFileNames());`）
    PkMap<int, int> other;
    other.insert(7, 70);
    other.insert(8, 80);

    PkMapIterator<int, int> c(m);
    c.next();
    c = PkMapIterator<int, int>(other);   // 换一份容器，游标重置到开头
    PkList<int> keys;
    while (c.hasNext()) {
        c.next();
        keys.append(c.key());
    }
    PK_VERIFY((keys == PkList<int>{7, 8}));

    // ③ Item 就是容器的 const_iterator（Qt 的 QMapIterator<K,T>::Item 同义）
    static_assert(std::is_same<PkMapIterator<int, int>::Item,
                               PkMap<int, int>::const_iterator>::value,
                  "PkMapIterator::Item 必须是容器的 const_iterator");
}

void PkJavaIteratorTest::hashIteratorKeyValue()
{
    // 迭代顺序未定义（内层 std::unordered_map，QHash 同样），只断言
    // "每个 key 恰好出现一次、key→value 对得上"。
    PkHash<int, int> h;
    h.insert(1, 10);
    h.insert(2, 20);
    h.insert(3, 30);

    PkHashIterator<int, int> it(h);
    PkList<int> keys;
    int rounds = 0;
    bool pairsMatch = true;
    while (it.hasNext()) {
        it.next();
        keys.append(it.key());
        if (it.value() != it.key() * 10) {
            pairsMatch = false;
        }
        ++rounds;
    }
    PK_COMPARE(rounds, 3);
    PK_VERIFY(pairsMatch);
    PK_COMPARE(keys.size(), 3);
    PK_VERIFY(keys.contains(1));
    PK_VERIFY(keys.contains(2));
    PK_VERIFY(keys.contains(3));

    // 持拷贝：构造后往原容器里加东西，迭代不受影响。
    // 用一份**新的**容器：上面那个 it 还活着，也持着 h 的一份拷贝，
    // 在 h 上断言引用计数会算进它，得到 3 而不是 2。
    PkHash<int, int> fresh;
    fresh.insert(1, 10);
    fresh.insert(2, 20);
    fresh.insert(3, 30);
    PkHashIterator<int, int> snapshot(fresh);
    PK_COMPARE(fresh.PkUseCount(), 2L);
    fresh.insert(4, 40);
    int snapshotRounds = 0;
    while (snapshot.hasNext()) {
        snapshot.next();
        ++snapshotRounds;
    }
    PK_COMPARE(snapshotRounds, 3);
    PK_COMPARE(fresh.size(), 4);

    // 空容器
    PkHash<int, int> empty;
    PkHashIterator<int, int> emptyIt(empty);
    PK_VERIFY(!emptyIt.hasNext());
}

void PkJavaIteratorTest::mutableMapIteratorSetValue()
{
    // libs/flake/svg/SvgStyleParser.cpp:527 的形态，逐行照抄：
    //   QMutableMapIterator<QString, QString> it(styleMap);
    //   while (it.hasNext()) { it.next(); if (it.value() == X) it.setValue(f(it.key())); }
    PkMap<int, int> m;
    m.insert(1, 10);
    m.insert(2, 20);
    m.insert(3, 20);

    // 迭代前先把容器拷一份，压"写回时正确 detach"
    PkMap<int, int> shadow(m);
    PK_VERIFY(m.PkIsSharedWith(shadow));

    PkMutableMapIterator<int, int> it(m);
    PkList<int> keys;
    while (it.hasNext()) {
        it.next();
        keys.append(it.key());
        if (it.value() == 20) {
            it.setValue(it.key() * 1000);
        }
    }

    // 顺序、key、写回结果
    PK_VERIFY((keys == PkList<int>{1, 2, 3}));
    PK_COMPARE(m.value(1), 10);
    PK_COMPARE(m.value(2), 2000);
    PK_COMPARE(m.value(3), 3000);
    PK_COMPARE(m.size(), 3);

    // 共享的另一份一个字节都没变
    PK_COMPARE(shadow.value(2), 20);
    PK_COMPARE(shadow.value(3), 20);
    PK_VERIFY(!m.PkIsSharedWith(shadow));

    // 写**最后一项**：setValue 之后重建的游标正好落在 end() 上，hasNext 必须为假
    PkMap<int, int> last;
    last.insert(1, 1);
    last.insert(2, 2);
    PkMutableMapIterator<int, int> lastIt(last);
    lastIt.next();
    lastIt.next();
    lastIt.setValue(222);
    PK_VERIFY(!lastIt.hasNext());
    PK_COMPARE(last.value(2), 222);
    PK_COMPARE(last.value(1), 1);

    // 连着 setValue 两次：第二次覆盖第一次，游标不乱
    PkMap<int, int> twice;
    twice.insert(1, 1);
    twice.insert(2, 2);
    PkMutableMapIterator<int, int> twiceIt(twice);
    twiceIt.next();
    twiceIt.setValue(11);
    twiceIt.setValue(111);
    PK_COMPARE(twice.value(1), 111);
    PK_VERIFY(twiceIt.hasNext());
    twiceIt.next();
    PK_COMPARE(twiceIt.key(), 2);

    // 空容器
    PkMap<int, int> empty;
    PkMutableMapIterator<int, int> emptyIt(empty);
    PK_VERIFY(!emptyIt.hasNext());
}

PK_TEST_MAIN(PkJavaIteratorTest)
