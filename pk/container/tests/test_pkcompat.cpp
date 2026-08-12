#include "PkCompatTest.h"

// ---------------------------------------------------------------------------
// include 顺序 A：**先 <QString>（pk/string 的垫片）后容器垫片**。
//
// 宏是文本替换，顺序不同展开时机不同 —— 两个顺序都要压。顺序 B（先容器后字符串）
// 在 PkCompatReverseOrder.cpp 那个 TU 里，两个 TU 链进同一个可执行文件。
//
// 这些 include 一律写成 `<...>` 的尖括号形式，与真实调用点一字不差 ——
// 解析到哪个文件完全由 -I 决定（CMakeLists.txt 里给 test_pkcompat 挂了
// pk/string/compat 与 pk/container/compat 两条路径）。
// ---------------------------------------------------------------------------
#include <QString>

#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QQueue>
#include <QSet>
#include <QStack>
#include <QStringList>
#include <QVector>

#include <cstdio>
#include <type_traits>

#include "pk_binder_PkCompatTest.inc"

// 顺序 B 的 TU（PkCompatReverseOrder.cpp）里的探针，返回 42 表示各项都成立。
int pkCompatReverseOrderProbe();

namespace {

// 输出改行缓冲：断言失败若伴随段错误，全缓冲的 stdout 会把崩溃前的行整段吞掉，
// 现场只剩一个不知道死在哪的 SIGSEGV。静态对象的构造先于 main。
struct PkLineBufferedStdout
{
    PkLineBufferedStdout() { std::setvbuf(stdout, nullptr, _IOLBF, 0); }
};
const PkLineBufferedStdout g_pkLineBuffered;

// 模板实参里的逗号会被预处理器当成宏参数分隔符，所以断言里一律用别名。
// **这些别名本身就是证据**：`QMap<QString, int>` 这一行要成立，
// 必须 QMap 与 QString 两个垫片同时生效。
using QMapStringInt = QMap<QString, int>;
using PkMapStringInt = PkMap<PkString, int>;
using QListString = QList<QString>;
using PkListString = PkList<PkString>;
using QHashStringInt = QHash<QString, int>;
using PkHashStringInt = PkHash<PkString, int>;
using QPairIntInt = QPair<int, int>;
using PkPairIntInt = PkPair<int, int>;
using QMapIteratorStringInt = QMapIterator<QString, int>;
using PkMapIteratorStringInt = PkMapIterator<PkString, int>;
using QMutableMapIteratorIntInt = QMutableMapIterator<int, int>;
using PkMutableMapIteratorIntInt = PkMutableMapIterator<int, int>;
using QHashIteratorIntInt = QHashIterator<int, int>;
using PkHashIteratorIntInt = PkHashIterator<int, int>;

// ---- 9 个容器名 ----
static_assert(std::is_same<QVector<int>, PkVector<int>>::value, "QVector → PkVector");
static_assert(std::is_same<QList<int>, PkList<int>>::value, "QList → PkList");
static_assert(std::is_same<QMapStringInt, PkMapStringInt>::value, "QMap → PkMap");
static_assert(std::is_same<QHashStringInt, PkHashStringInt>::value, "QHash → PkHash");
static_assert(std::is_same<QSet<int>, PkSet<int>>::value, "QSet → PkSet");
static_assert(std::is_same<QStringList, PkStringList>::value, "QStringList → PkStringList");
static_assert(std::is_same<QPairIntInt, PkPairIntInt>::value, "QPair → PkPair");
static_assert(std::is_same<QStack<int>, PkStack<int>>::value, "QStack → PkStack");
static_assert(std::is_same<QQueue<int>, PkQueue<int>>::value, "QQueue → PkQueue");

// ---- Java 风格迭代器（跟着容器头走）----
static_assert(std::is_same<QVectorIterator<int>, PkVectorIterator<int>>::value,
              "QVectorIterator → PkVectorIterator（由 <QVector> 提供）");
static_assert(std::is_same<QListIterator<int>, PkListIterator<int>>::value,
              "QListIterator → PkListIterator（由 <QList> 提供）");
static_assert(std::is_same<QMutableListIterator<int>, PkMutableListIterator<int>>::value,
              "QMutableListIterator → PkMutableListIterator（由 <QList> 提供）");
static_assert(std::is_same<QMapIteratorStringInt, PkMapIteratorStringInt>::value,
              "QMapIterator → PkMapIterator（由 <QMap> 提供）");
static_assert(std::is_same<QMutableMapIteratorIntInt, PkMutableMapIteratorIntInt>::value,
              "QMutableMapIterator → PkMutableMapIterator（由 <QMap> 提供）");
static_assert(std::is_same<QHashIteratorIntInt, PkHashIteratorIntInt>::value,
              "QHashIterator → PkHashIterator（由 <QHash> 提供）");

// ---- 宏是**全词** token 替换：QList 与 QListIterator 是两个 token ----
//
// `#define QList PkList` 不会把 `QListIterator` 改写成 `PkListIterator`
// ——所以每个迭代器名都得显式写一条 #define。这条断言把它钉住：两个名字确实
// 各自映射到各自的目标，而不是"QListIterator 被拆成 PkList + Iterator"。
static_assert(!std::is_same<QListIterator<int>, QList<int>>::value,
              "QListIterator 与 QList 必须是两个不同的名字");

// ---- 带 QString 的组合类型（brief 指名要压的三条）----
static_assert(std::is_same<QMap<QString, int>, PkMap<PkString, int>>::value, "");
static_assert(std::is_same<QListString, PkListString>::value, "");
static_assert(std::is_same<QHash<QString, int>, PkHash<PkString, int>>::value, "");

// QStringList 与 QList<QString> 的关系：派生，不是同一个类型（Qt 一致）
static_assert(std::is_base_of<QListString, QStringList>::value,
              "QStringList 必须派生自 QList<QString>");
static_assert(!std::is_same<QStringList, QListString>::value,
              "QStringList 不是 QList<QString> 的别名");

// 派生容器的基类关系照 Qt
static_assert(std::is_base_of<QVector<int>, QStack<int>>::value, "QStack : QVector");
static_assert(std::is_base_of<QList<int>, QQueue<int>>::value, "QQueue : QList");

// 带析构计数器的类型，供 qDeleteAll 用
struct PkCompatDeleteProbe
{
    ~PkCompatDeleteProbe() { ++s_destroyed; }
    static int s_destroyed;
};

int PkCompatDeleteProbe::s_destroyed = 0;

} // namespace

void PkCompatTest::containerShimsResolve()
{
    // static_assert 已经把类型等价性钉死了（编得过就是证明）。这里补一遍
    // **运行期真的能用** —— 垫片映射对了但类型不可用的话，上面全会编不过，
    // 所以这一段更多是给读者看"调用点原样写就是这个样子"。
    QVector<int> vec;
    vec << 1 << 2 << 3;
    PK_COMPARE(vec.size(), 3);

    QList<int> list;
    list.append(4);
    PK_COMPARE(list.size(), 1);

    QMap<QString, int> map;
    map.insert(QStringLiteral("a"), 1);
    PK_COMPARE(map.size(), 1);
    PK_COMPARE(map.value(QStringLiteral("a")), 1);

    QHash<QString, int> hash;
    hash.insert(QStringLiteral("b"), 2);
    PK_COMPARE(hash.value(QStringLiteral("b")), 2);

    QSet<int> set;
    set.insert(9);
    PK_VERIFY(set.contains(9));

    QStringList sl;
    sl << QStringLiteral("x") << QStringLiteral("y");
    const QString joined = sl.join(QStringLiteral(","));
    PK_COMPARE(joined, QString("x,y"));

    QPair<int, int> pair = qMakePair(1, 2);
    PK_COMPARE(pair.first, 1);

    QStack<int> stack;
    stack.push(5);
    PK_COMPARE(stack.top(), 5);

    QQueue<int> queue;
    queue.enqueue(6);
    PK_COMPARE(queue.head(), 6);
}

void PkCompatTest::javaIteratorShimsResolve()
{
    // 调用点原样的写法：`#include <QMap>` 之后直接用 QMapIterator。
    QMap<QString, int> map;
    map.insert(QStringLiteral("a"), 1);
    map.insert(QStringLiteral("b"), 2);

    QMapIterator<QString, int> it(map);
    int sum = 0;
    int rounds = 0;
    while (it.hasNext()) {
        it.next();
        sum += it.value();
        ++rounds;
    }
    PK_COMPARE(rounds, 2);
    PK_COMPARE(sum, 3);

    QList<int> list;
    list << 1 << 2 << 3;
    QListIterator<int> listIt(list);
    int listSum = 0;
    while (listIt.hasNext()) {
        listSum += listIt.next();
    }
    PK_COMPARE(listSum, 6);

    QMutableListIterator<int> mutIt(list);
    while (mutIt.hasNext()) {
        if (mutIt.next() == 2) {
            mutIt.remove();
        }
    }
    PK_COMPARE(list.size(), 2);

    QVector<int> vec;
    vec << 10 << 20;
    QVectorIterator<int> vecIt(vec);
    vecIt.toBack();
    PK_COMPARE(vecIt.previous(), 20);

    QHash<int, int> hash;
    hash.insert(1, 100);
    QHashIterator<int, int> hashIt(hash);
    PK_VERIFY(hashIt.hasNext());
    hashIt.next();
    PK_COMPARE(hashIt.key(), 1);
    PK_COMPARE(hashIt.value(), 100);

    QMap<int, int> mutMap;
    mutMap.insert(1, 1);
    QMutableMapIterator<int, int> mutMapIt(mutMap);
    mutMapIt.next();
    mutMapIt.setValue(99);
    PK_COMPARE(mutMap.value(1), 99);
}

void PkCompatTest::foreachAndDeleteAllArriveThroughContainerHeaders()
{
    // 本 TU 从头到尾没有 `#include <QtGlobal>` —— Q_FOREACH / foreach /
    // qDeleteAll 全是容器垫片经 PkContainerAlgo.h 传递进来的。真实调用点正是
    // 这个样子（极少有人单独 include <QtGlobal>）。
    QVector<int> v;
    v << 1 << 2 << 3;

    int sumUpper = 0;
    Q_FOREACH (int x, v) {
        sumUpper += x;
    }
    PK_COMPARE(sumUpper, 6);

    int sumLower = 0;
    foreach (int x, v) {
        sumLower += x;
    }
    PK_COMPARE(sumLower, 6);

    // 遍历 QStringList（1547 处 QStringList 里最常见的用法）
    QStringList sl;
    sl << QStringLiteral("a") << QStringLiteral("bb");
    int totalLength = 0;
    Q_FOREACH (const QString &s, sl) {
        totalLength += s.size();
    }
    PK_COMPARE(totalLength, 3);

    // 遍历 QMap<QString, int> 得到的是 value
    QMap<QString, int> map;
    map.insert(QStringLiteral("k1"), 10);
    map.insert(QStringLiteral("k2"), 20);
    int mapSum = 0;
    Q_FOREACH (int x, map) {
        mapSum += x;
    }
    PK_COMPARE(mapSum, 30);

    // qDeleteAll
    PkCompatDeleteProbe::s_destroyed = 0;
    QList<PkCompatDeleteProbe *> owned;
    owned.append(new PkCompatDeleteProbe);
    owned.append(new PkCompatDeleteProbe);
    qDeleteAll(owned);
    PK_COMPARE(PkCompatDeleteProbe::s_destroyed, 2);
    owned.clear();
}

void PkCompatTest::crossDirectoryWithStringShim()
{
    // pk/string/compat/QString 与 pk/container/compat/QMap 同时生效时，
    // QMap<QString, X> 必须解析成 PkMap<PkString, X>。static_assert 已经钉住
    // 类型，这里压它真的能工作 —— 尤其是 std::map<PkString, V> 需要
    // PkString::operator<（有序容器的比较），那条链路只有实例化才压得到。
    QMap<QString, int> map;
    map.insert(QStringLiteral("banana"), 2);
    map.insert(QStringLiteral("apple"), 1);
    map.insert(QStringLiteral("cherry"), 3);
    PK_COMPARE(map.size(), 3);

    // std::map 按 key 升序 → PkString::operator< 的逐码元序
    QList<QString> keys = map.keys();
    PK_COMPARE(keys.size(), 3);
    const QString firstKey = keys.at(0);
    const QString lastKey = keys.at(2);
    PK_COMPARE(firstKey, QString("apple"));
    PK_COMPARE(lastKey, QString("cherry"));

    // QList<QString> → QStringList 的隐式转换（调用点大量依赖）
    QStringList asStringList = keys;
    PK_COMPARE(asStringList.size(), 3);
    const QString joined = asStringList.join(QStringLiteral("|"));
    PK_COMPARE(joined, QString("apple|banana|cherry"));

    // QMapIterator<QString, int>：两个垫片同时作用在同一个模板实参列表上
    QMapIterator<QString, int> it(map);
    int sum = 0;
    while (it.hasNext()) {
        it.next();
        sum += it.value();
    }
    PK_COMPARE(sum, 6);
}

void PkCompatTest::reverseIncludeOrderAlsoWorks()
{
    // 顺序 B（先容器垫片后 <QString>）在 PkCompatReverseOrder.cpp 里，
    // 那个 TU 的 static_assert 编得过 + 这里拿到 42 = 两个顺序都成立。
    PK_COMPARE(pkCompatReverseOrderProbe(), 42);
}

void PkCompatTest::hashOfStringInstantiates()
{
    // PkHash<PkString, X> / PkSet<PkString> 要求 qHash(const PkString &)，
    // 那条重载是 pk/container/PkStringHash.h 补的（不是 pk/string 里的，
    // 那会越界）。**编得过并且能查得到 = ADL / 非限定查找链路没断。**
    QHash<QString, int> hash;
    hash.insert(QStringLiteral("alpha"), 1);
    hash.insert(QStringLiteral("beta"), 2);
    PK_COMPARE(hash.size(), 2);
    PK_COMPARE(hash.value(QStringLiteral("alpha")), 1);
    PK_COMPARE(hash.value(QStringLiteral("beta")), 2);
    PK_COMPARE(hash.value(QStringLiteral("missing")), 0);

    QSet<QString> set;
    set.insert(QStringLiteral("x"));
    set.insert(QStringLiteral("x"));
    set.insert(QStringLiteral("y"));
    PK_COMPARE(set.size(), 2);
    PK_VERIFY(set.contains(QStringLiteral("y")));

    // 相等的字符串必须落到同一个桶（哈希与 operator== 自洽）
    QString a = QStringLiteral("same");
    QString b = QString("same");
    PK_VERIFY(a == b);
    QHash<QString, int> selfConsistent;
    selfConsistent.insert(a, 1);
    selfConsistent.insert(b, 2);
    PK_COMPARE(selfConsistent.size(), 1);
    PK_COMPARE(selfConsistent.value(a), 2);

    // QHashIterator<QString, int> 也要能实例化
    QHashIterator<QString, int> it(hash);
    int rounds = 0;
    while (it.hasNext()) {
        it.next();
        ++rounds;
    }
    PK_COMPARE(rounds, 2);
}

PK_TEST_MAIN(PkCompatTest)
