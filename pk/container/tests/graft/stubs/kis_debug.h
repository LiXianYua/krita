#pragma once

// `libs/global/kis_debug.h` 的最小替身。
//
// 唯一的真实需求在 `kis_fill_interval.h:57`：
//     inline QDebug operator<<(QDebug dbg, const KisFillInterval& i)
// —— 一个**永远不会被调用**（本试接零调用点）但**必须编过**的自由函数。
// 它按值收 `QDebug`、调 `dbg.nospace()`、往里塞 `const char*` 与 `int`。
// 所以替身只需要一个可拷贝、有 `nospace()`、`operator<<` 吃任意东西的空壳。
//
// ── 为什么这里要 include 一串容器头 ──────────────────────────
// 真 Qt 的 `<QDebug>`（qdebug.h:44-53）传递 include 了 qhash.h / qlist.h /
// qmap.h / qpair.h / qvector.h / qset.h —— 因为它要给这些容器提供流插入重载。
// **被测实现正是靠这条传递性拿到 `QHash` 的**：`kis_fill_interval_map_p.h`
// 里 `typedef QHash<int, LineIntervalMap> GlobalMap;` 而那个头自己不 include
// `<QHash>`，`kis_fill_interval_map.h` 也只 include 了 `<QMap>`/`<QStack>`。
//
// 不复刻这条传递性的话，`_p.h` 会因为"QHash 未声明"编不过 —— 而那**不是调用点
// 的问题**，是 Qt 的传递 include 关系没被复刻全。pk/test 的 `compat/QObject`
// 对 `<QtGlobal>`/`qAbs` 做的是同一件事，同一条理由。
#include <QHash>
#include <QList>
#include <QMap>
#include <QPair>
#include <QSet>
#include <QVector>

#include "kis_assert.h"

class QDebug
{
public:
    QDebug &nospace() { return *this; }
    QDebug &space() { return *this; }
    QDebug &noquote() { return *this; }

    template <typename T>
    QDebug &operator<<(const T &) { return *this; }
};

// 真品的调试宏族（dbgKrita / warnKrita / ...）在本试接里零调用点，不给。
// 判据①「一项不多一项不少」对脚手架同样适用：多给一个就是多一个没人压的形状。
