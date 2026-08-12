// ---------------------------------------------------------------------------
// include 顺序 B：**先容器垫片，后 <QString>**。
//
// 与 test_pkcompat.cpp 的顺序 A 正好相反。垫片是 `#define`，是文本替换，
// **顺序不同展开时机不同** —— 一个顺序能编过不代表另一个也能，所以两个顺序各占
// 一个 TU，链进同一个可执行文件。本文件没有 main，也没有测试类，只有
// static_assert 与一个把结果带回顺序 A 那边的探针函数。
//
// 编得过 = 顺序 B 成立；pkCompatReverseOrderProbe() 返回 42 = 运行期也对得上。
// ---------------------------------------------------------------------------

#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QQueue>
#include <QSet>
#include <QStack>
#include <QStringList>
#include <QVector>

// 字符串垫片最后才进来
#include <QString>

#include <type_traits>

namespace {

// 模板实参里的逗号在 static_assert 里其实没问题（static_assert 是关键字不是宏），
// 但与顺序 A 那个 TU 保持同样的写法，方便逐条对读。
using QMapStringInt = QMap<QString, int>;
using PkMapStringInt = PkMap<PkString, int>;
using QHashStringInt = QHash<QString, int>;
using PkHashStringInt = PkHash<PkString, int>;
using QPairIntInt = QPair<int, int>;
using PkPairIntInt = PkPair<int, int>;
using QMapIteratorStringInt = QMapIterator<QString, int>;
using PkMapIteratorStringInt = PkMapIterator<PkString, int>;

static_assert(std::is_same<QVector<int>, PkVector<int>>::value, "QVector → PkVector");
static_assert(std::is_same<QList<int>, PkList<int>>::value, "QList → PkList");
static_assert(std::is_same<QMapStringInt, PkMapStringInt>::value, "QMap → PkMap");
static_assert(std::is_same<QHashStringInt, PkHashStringInt>::value, "QHash → PkHash");
static_assert(std::is_same<QSet<int>, PkSet<int>>::value, "QSet → PkSet");
static_assert(std::is_same<QStringList, PkStringList>::value, "QStringList → PkStringList");
static_assert(std::is_same<QPairIntInt, PkPairIntInt>::value, "QPair → PkPair");
static_assert(std::is_same<QStack<int>, PkStack<int>>::value, "QStack → PkStack");
static_assert(std::is_same<QQueue<int>, PkQueue<int>>::value, "QQueue → PkQueue");

// 这三条是 brief 指名要在**两个顺序**下都成立的
static_assert(std::is_same<QMap<QString, int>, PkMap<PkString, int>>::value, "");
static_assert(std::is_same<QList<QString>, PkList<PkString>>::value, "");
static_assert(std::is_same<QHash<QString, int>, PkHash<PkString, int>>::value, "");

// 迭代器名同样要在顺序 B 下解析正确
static_assert(std::is_same<QListIterator<int>, PkListIterator<int>>::value, "");
static_assert(std::is_same<QMutableListIterator<int>, PkMutableListIterator<int>>::value, "");
static_assert(std::is_same<QVectorIterator<int>, PkVectorIterator<int>>::value, "");
static_assert(std::is_same<QMapIteratorStringInt, PkMapIteratorStringInt>::value, "");
static_assert(std::is_same<QHashIterator<int, int>, PkHashIterator<int, int>>::value, "");
static_assert(std::is_same<QMutableMapIterator<int, int>,
                           PkMutableMapIterator<int, int>>::value, "");

} // namespace

// 运行期探针：跑一遍 QMap<QString,int> + QHash<QString,int> + Q_FOREACH，
// 全对就返回 42。返回别的数值就说明顺序 B 下某一条虽然编过了却不等价。
int pkCompatReverseOrderProbe()
{
    QMap<QString, int> map;
    map.insert(QStringLiteral("a"), 1);
    map.insert(QStringLiteral("b"), 2);

    int sum = 0;
    Q_FOREACH (int v, map) {
        sum += v;
    }
    if (sum != 3) {
        return -1;
    }

    // PkHash<PkString, X> 在顺序 B 下也要能实例化（qHash 的查找链路）
    QHash<QString, int> hash;
    hash.insert(QStringLiteral("k"), 40);
    if (hash.value(QStringLiteral("k")) != 40) {
        return -2;
    }

    QStringList sl;
    sl << QStringLiteral("x") << QStringLiteral("y");
    if (sl.join(QStringLiteral("")) != QString("xy")) {
        return -3;
    }

    QMapIterator<QString, int> it(map);
    int rounds = 0;
    while (it.hasNext()) {
        it.next();
        ++rounds;
    }
    if (rounds != 2) {
        return -4;
    }

    return 42;
}
