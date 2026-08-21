// coexist_qt_probe.cpp —— R-34「让位给真 Qt」守卫的共存探针。
//
// 同一个源文件两种编译模式，由 coexist_qt_probe.sh 各编一次、各跑一次：
//   模式 A（real Qt 已进 TU，QT_CORE_LIB 定义）：
//     g++ ... -I$QT/include{/QtCore,/QtGui,/QtWidgets,/QtTest} -I pk/signal -I pk/concurrent \
//            -DQT_CORE_LIB -DQT_GUI_LIB -DQT_WIDGETS_LIB -DQT_TESTLIB_LIB
//     static_assert 确认：QObject 保持真 Qt、QOverload 用真 Qt 的、
//     PkMetaObject::Connection == PkConnection；main 里用真 Qt QObject::connect。
//   模式 B（无 Qt）：
//     g++ ... -I pk/signal -I pk/concurrent
//     static_assert 确认：compat/QObject 照常定义 QObject→PkObject、
//     namespace Qt::DirectConnection == PkConnectionType::Direct、QOverload 用 pk 的；
//     main 里构造 PkObject。
//
// 两个模式都要 exit 0。能编过本身就是断言的一半——重复定义 QOverload/QMetaObject/
// namespace Qt 是硬错误，宏冲突会把函数名当场改写坏。另一半（取值与真 Qt 对齐）由
// pk/signal 与 pk/log 的单测各自钉住；这里只证明「让位之后两种模式都成立」。

#if defined(QT_CORE_LIB)
#include <QTest>
#include <QStandardPaths>
#include <QLocale>
#endif

#include <compat/QObject>
#include <PkObject.h>
#include <PkConnect.h>
#include <PkPointer.h>

#if defined(QT_CORE_LIB)
// ── 模式 A：real Qt 在场 ─────────────────────────────────────────────
// real Qt 类型必须保持 real Qt 类型（QObject 宏不得触发）。
static_assert(std::is_same<QObject, ::QObject>::value,
              "QObject macro must NOT fire under real Qt");
static_assert(!std::is_same<QObject, PkObject>::value,
              "QObject must not alias PkObject under real Qt");
// pk 类型必须仍可用（KisSynchronizedConnection 等消费方依赖）。
static_assert(std::is_same<PkMetaObject::Connection, PkConnection>::value,
              "PkMetaObject::Connection == PkConnection");
// QOverload 必须是真 Qt 的（对 &C::sig 重载消歧）。
struct ProbeS { void sig(int) {} void sig(const QString&) {} };
using ProbePtr = void (ProbeS::*)(int);
constexpr ProbePtr probeP = QOverload<int>::of(&ProbeS::sig);
static_assert(probeP != nullptr, "QOverload from real Qt must work");

int main() {
    // QObject 构造与 connect 不需要 display（不用 QApplication），无头环境直接可跑。
    QObject a, b;
    QObject::connect(&a, &QObject::destroyed, &b, [](){});
    return 0;
}
#else
// ── 模式 B：无 Qt（正常 pk 用法）──────────────────────────────────────
// compat/QObject 照常定义 QObject→PkObject。
static_assert(std::is_same<QObject, PkObject>::value,
              "QObject must alias PkObject in no-Qt mode");
static_assert(std::is_same<PkMetaObject::Connection, PkConnection>::value,
              "PkMetaObject::Connection == PkConnection");
static_assert(Qt::DirectConnection == PkConnectionType::Direct,
              "namespace Qt ConnectionType works");
struct ProbeS { void sig(int) {} };
using ProbePtr = void (ProbeS::*)(int);
constexpr ProbePtr probeP = QOverload<int>::of(&ProbeS::sig);
static_assert(probeP != nullptr, "QOverload works in no-Qt mode");

int main() {
    // 不构造 PkObject（构造在 PkObject.cpp，需链 pk 库；共存是编译期性质，
    // 上面 static_assert 已钉住 compat 定义）。运行时只校验 compat 的
    // namespace Qt 常量确实可用。
    return (Qt::DirectConnection == PkConnectionType::Direct) ? 0 : 1;
}
#endif
