#pragma once

#include <QObject>

// R-53 Task 1 —— 跨镜像 any_cast 回归用例的数据驱动壳。
// 形制照 tests/variant_case.h：Q_OBJECT + private Q_SLOTS，由 pk_test_generate
// 产出 pk_binder_cross_image_case.inc 给 test_cross_image_payload.cpp #include。
//
// **槽名 = 负载类型名本身**（plan §4.1 那 14 个）：红证据里失败用例名就是那 11 个
// 未挂属性的类型，绿证据里 14 个全过——名字对照读得出来，不需要另设映射表。
//
// 最后一个槽不是用例，是 unique 位的运行期实测打印（恒过，只出证据）。
class CrossImageCase : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── S-17 已修（对照组：红段与绿段都必须绿）──────────────────────
    void PkString();
    void PkStringList();
    void PkByteArray();

    // ── 本任务范围（Task 1 未挂属性 ⇒ 红段必须红；Task 2 挂属性后转绿）──
    void PkPoint();
    void PkPointF();
    void PkRect();
    void PkRectF();
    void PkSize();
    void PkSizeF();
    void PkLine();
    void PkLineF();
    void PkDate();
    void PkTime();
    void PkDateTime();

    // ── 证据：两侧 type_info 的 unique 位实测（plan §1.3 / §5 Task 1）──
    void typeInfoUniqueBit();
};
