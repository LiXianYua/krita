// pk/xml/tests/graft/graft_run_b.sh 的构建期胶水，不是对 Krita 源树的改动。
// 把真实、未修改的测试 .cpp 整个拉进来——PkTestBinder<T> 的显式特化必须与
// qExec<T> 实例化点（SIMPLE_TEST_MAIN 展开出的 main()）在同一个翻译单元里
// 看见，而测试 .cpp 本身不许改一个字节去加这行 include，所以只能反过来由
// 这层不属于源树的 driver.cpp 去 include 它——形态照抄
// pk/xml/tests/graft/graft_run_a_driver.cpp（候选 A）。
//
// 与候选 A 的 driver 不同：候选 B 不需要"外挂函数体"这一段——
// kis_distance_information_test.cpp 只依赖 KisDistanceInitInfo/
// KisDistanceInformation（候选源 kis_distance_information.cpp 本身，已原地
// 编译）与 KisPaintInformation/KisAlgebra2D（stubs/brushengine/
// kis_paint_information.h 编译期占位 + libs/global/kis_algebra_2d.h 真品的
// inline 模板），没有一个像 KisTimeSpan::saveValue/loadValue 那样"声明在别处、
// 定义拖着无关依赖树"的缺口。
#include "libs/image/tests/kis_distance_information_test.cpp"
#include "binder.inc"

// `KisAlgebra2D::directionBetweenPoints`（真品声明在 libs/global/
// kis_algebra_2d.h:220，实现在 kis_algebra_2d.cpp——那个 .cpp 没有被本试接
// 编译，只声明会在链接期报 undefined reference）的编译期占位实现。唯一调用点
// 是本翻译单元里 `kis_distance_information_test.cpp` 的 `testInterpolation()`
// ——`graft_run_b.sh` 用 pk/test harness 的命令行过滤只跑 `testInitInfo`
// 这一个 slot（覆盖 brief 要求的 testInitInfoXMLClone），`testInterpolation`
// 从未被执行到，这里只补链接期符号，返回值不追求真实的两点间方向角计算，
// 与 stubs/QVector2D::length() 的处置同一原则。
//
// 放在这里而不是共用的 stubs/graft_stubs.cpp：那份文件是候选 A/B 共用的
// 实现侧，graft_run_a.sh 编译它时不带 FORCE 数组（候选 A 从不需要
// kis_algebra_2d.h 要求的 QList/QDebug 提前就位）——试过放在那边，候选 A 的
// 构建当场报一堆 QList/QDebug 相关编译错误（回归，已在报告里记录）。这个
// driver.cpp 本身已经带 FORCE 编译（graft_run_b.sh 的 ②.5 段），kis_algebra_2d.h
// 早已通过 kis_distance_information_test.cpp 的 include 链被解析过一次，
// 这里直接补定义不会重新触发同一批问题。
namespace KisAlgebra2D {
qreal directionBetweenPoints(const QPointF &, const QPointF &, qreal defaultAngle)
{
    return defaultAngle;
}
}
