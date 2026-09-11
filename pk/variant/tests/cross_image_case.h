#pragma once

#include <QObject>

// R-53 Task 1 —— 跨镜像 any_cast 回归用例的数据驱动壳。
// 形制照 tests/variant_case.h：Q_OBJECT + private Q_SLOTS，由 pk_test_generate
// 产出 pk_binder_cross_image_case.inc 给 test_cross_image_payload.cpp #include。
//
// **槽名 = 负载类型名本身**（plan §4.1 的 14 个 + 轮 2 新增的 3 个容器类型，共 17 个）：
// 红证据里失败用例名就是未挂属性的类型，绿证据里 17 个全过——名字对照读得出来，不需要另设映射表。
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

    // ── 几何/时间类型（属性已挂 ⇒ 恒绿；轮 2 前是红段的一部分）──
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

    // ── 轮 2 新增（A 裁决：std 容器 typedef，属性落点在 PkVariant 自己；轮 3 已挂 ⇒ 全绿）──
    void PkVariantList();
    void PkVariantHash();
    void PkVariantMap();

    // ── 证据：两侧 type_info 的 unique 位实测（plan §1.3 / §5 Task 1）──
    void typeInfoUniqueBit();
};
