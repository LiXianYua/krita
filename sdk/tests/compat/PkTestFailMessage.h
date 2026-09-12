#pragma once
// ===========================================================================
// sdk/tests/compat/PkTestFailMessage.h  （R-75 新增）
// ---------------------------------------------------------------------------
// 为什么需要它：真 Qt 的 `QFAIL` 展开成
//     QTest::qFail(static_cast<const char *>(message), __FILE__, __LINE__);
// （逐字：$QT/lib/QtTest.framework/Versions/5/Headers/qtestcase.h:69-73），
// 它靠 **QByteArray 的 `operator const char *() const`**
// （$QT/lib/QtCore.framework/Versions/5/Headers/qbytearray.h:207）接受
// `QFAIL(QString(...).toLatin1())` 这种形态。
// **真 Qt 编译探针实测**（R-75 task-2 报告 §3.4）：
//     const char*            ✅  ·  QByteArray  ✅
//     QString(…).toLatin1()  ✅  ·  std::string ❌（cannot cast from type
//                                    'std::string' to pointer type 'const char *'）
//
// pk 侧 `QFAIL` → `PK_FAIL`（pk/test/compat/QTest:22 的 `#define QFAIL PK_FAIL`），
// 而 `PK_FAIL` 把 message **直接**交给
//     PkTestCase::checkResult(bool, const char*, int, const std::string&)
// （pk/test/PkTest.h:87-93 的宏 → pk/test/PkTestCase.h:41 的签名）。
// `PkByteArray`（pk/container/PkByteArray.h）**既无 `operator const char*`
// 也无 `operator std::string`** ⇒ 真实测试源里 47 处 `QFAIL`
// （`grep -rn QFAIL libs/image/tests/*.cpp`，其中 `QFAIL(QString(...).toLatin1())`
// 为一个子集）在 pk 栈上全数编不过。
//
// 做法：把 message **先归一成 std::string** 再交给 checkResult。下面四条重载
// 覆盖 PkByteArray / PkString / const char* / std::string。
//   注：`std::string` 一条**比真 Qt 宽**（真 Qt 的 QFAIL 拒收 std::string，见上）。
//   宽出来的方向不会让任何真实测试源在 pk 栈「编得过而真 Qt 编不过」——因为真
//   Qt 拒收 std::string，树里根本不存在把 std::string 交给 QFAIL 的调用点；
//   这两条只是让归一函数在四类入参上语义完整（R-75 brief §4 逐字要求四种）。
//
// **只对 pk 测试 TU 生效**：本头只被 `sdk/tests/PkTestCompatAll.h` include，而
// 后者只被 `kritatestsdk_pk` 以 `-include` 强制注入 pk_add_test 目标的 TU。真 Qt
// 测试栈 `kritatestsdk` 既**不定义 `KRITA_TESTSDK_PK_NATIVE`**、也**不 include
// `PkTestCompatAll.h`**，所以拿不到本头，行为一个字不变。
// ===========================================================================

#include "PkString.h"   // 带进 PkByteArray（PkString.h:8 已 include 它）

#include <cstddef>
#include <string>

// QByteArray 的等价物：按 (constData, size) 取字节（不带 NUL，constData() 空时
// 返回非空 NUL 指针，size()==0 ⇒ 空串）。
inline std::string pkTestFailMessage(const PkByteArray &message)
{
    return std::string(message.constData(),
                       static_cast<std::size_t>(message.size()));
}

// QString 的等价物：PkString::PkToUtf8() 直返 std::string。
inline std::string pkTestFailMessage(const PkString &message)
{
    return message.PkToUtf8();
}

// C 串：nullptr 按空串（与 PkTest.h 里 qFail 的「nullptr 当空串」口径一致）。
inline std::string pkTestFailMessage(const char *message)
{
    return message ? std::string(message) : std::string();
}

// std::string：原样（见头注「比真 Qt 宽」的说明）。
inline std::string pkTestFailMessage(const std::string &message)
{
    return message;
}
