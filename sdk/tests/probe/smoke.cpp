// S-00 符号层零 Qt 证据探针。只 #include Task 2/3 剥离完的两个头并实例化
// 关键符号，编成静态库后 `nm -u <lib>.a | grep -i qt` 应为空。
//
// 不包含 qimage_test_util.h / simpletest.h —— 它们本轮仍是 Qt 代码
// （见 sdk/tests/README.md「已知缺口」），编进来会把 Qt5::Test/Qt5::Widgets
// 混进探针，污染这份"零 Qt"证明的意义。
//
// 两处孤立编译才会暴露的隐式依赖（在完整 Krita 构建里靠同一 TU 内别的头顺带
// 带出来，孤立编译时必须显式补，都只在本文件范围内解决，不改 Task 2/3 的
// 两个头）：
//
//   ① kis_assert.h（KisRectsCollisionsTracker.h 间接 #include）无条件
//      `#include <QtGlobal>` + `#include <kritaglobal_export.h>`。前者只为了
//      `qt_noop()`，后者是 CMake generate_export_header() 的构建期产物、源码树
//      里不存在——两个垫片都放在 stubs/，见该目录下两份文件各自的注释。
//
//   ② ENTER_FUNCTION() 是 libs/global/kis_debug.h 定义的宏，但
//      KisRectsCollisionsTracker.h 并不 #include 那个头（原始 Krita 里靠隐式
//      include 顺序）。kis_debug.h 本身 `#include <QDebug>` + `<QLoggingCategory>`
//      （真 Qt），不能为了这一个宏把它整个拉进来——那会直接污染这份证明。
//      这里按它在真品里的定义（`qDebug() << "Entering" << 函数名`）用探针自己
//      已经落地的 qDebug（pk/log/PkMessageLogger.h + pk/log/compat/QDebug）
//      本地重定义一份等价物。

#include "PkMessageLogger.h"
#include "compat/QDebug"

#define ENTER_FUNCTION() qDebug() << "Entering" << "<S-00 probe>"

#include "../KisMeasureAvgPortion.h"
#include "../KisRectsCollisionsTracker.h"

void s00ProbeInstantiate()
{
    KisRectsCollisionsTracker tracker;
    PkRect r(0, 0, 4, 4);
    tracker.startAccessingRect(r);
    tracker.endAccessingRect(r);

    TestUtil::PerObjectMetric<int> metric;
    metric.startFrame(1);
    metric.endFrame(1);
}
