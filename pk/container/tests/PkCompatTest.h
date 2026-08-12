#pragma once

// compat/ 垫片的单测：证明真实调用点的 `#include <QVector>` / `<QMap>` / …
// 在一个字都不改的前提下解析到对应的 Pk 类型。函数定义在 test_pkcompat.cpp
// 与 PkCompatReverseOrder.cpp（后者只为了压 include 顺序，没有 main）。
//
// 垫片是**宏**，宏是文本替换，所以"证明它生效"的主力是 static_assert：
// 名字没被 #define 的话，`QVector<int>` 这一行根本就编不过；被 #define 错了的话，
// is_same 会当场判假。运行期用例只补两件 static_assert 表达不了的事：
// 两个 include 顺序都成立，以及 PkHash<PkString,X> 真的能实例化（qHash 的
// ADL 链路没断）。
#include <QObject>

class PkCompatTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // 9 个容器名各自映射到 Pk 类型
    void containerShimsResolve();
    // Java 风格迭代器的名字跟着它在 Qt 里的归属走（<QMap> 给 QMapIterator，等等）
    void javaIteratorShimsResolve();
    // Q_FOREACH / foreach / qDeleteAll 靠容器头传递进来（调用点不单独 include <QtGlobal>）
    void foreachAndDeleteAllArriveThroughContainerHeaders();
    // 与 pk/string 垫片同时生效：QMap<QString,X> / QList<QString> / QHash<QString,X>
    void crossDirectoryWithStringShim();
    // 反过来的 include 顺序（先容器后字符串）在另一个 TU 里压，这里只取结果
    void reverseIncludeOrderAlsoWorks();
    // PkHash<PkString, X> / PkSet<PkString> 真的能实例化并工作
    void hashOfStringInstantiates();
};
