#pragma once

// PkColor（对齐 Qt 5.15.7 QColor）的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。这里走 pk/global
// 的等价展开（局部 #define），不走 pk/test/compat —— 本模块的 compat 垫片只给
// graft/difftest 用，单测编译行不含 pk/test/compat。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"
#include "../../PkColor.h"

// ---------------------------------------------------------------------------
// 所有期望值都取自**真 Qt 5.15.7** qcolor.cpp / qcolor.h 的源码对照
// （/tmp/qcolor515.cpp）与本任务 probe 的实测输出。对齐口径：与 Qt 的任何行为
// 差异默认都是缺陷，Qt 那些看着像 bug 的地方也照抄（HSL(0,255,128) → (255,1,1)、
// operator== 比较 alpha、setHsvF 越界静默 return、fromHsv(360) 返回无效等）。
// 相对 brief 示例的修正（brief 的探针把 Qt::green 写成 (0,128,0)、把
// Qt::darkYellow 写成无效、断言 operator== 忽略 alpha —— 均与真 Qt 不符）在
// README「偏离登记」逐条声明，这里全部用真 Qt 的取值。
//
// 数据驱动族试验（brief Step 4b）：globalColor / namedColor / hexColor /
// hsvRoundtrip 四族，每族一个 _data 槽 + 一个取数槽，逐行 PK_COMPARE。
// ---------------------------------------------------------------------------

class PkColorCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 非数据驱动：构造 / 状态 ─────────────────────────
    void defaultCtor();
    void rgbCtor();
    void rgbCtorOutOfRange();
    void rgbCtorAlpha();
    void copyAssign();
    void wireStateIsLossless();
    void setRgbFClearsExtendedWirePad();

    // ── 数据驱动族试验 ─────────────────────────────────
    void globalColor_data();
    void globalColor();
    void namedColor_data();
    void namedColor();
    void hexColor_data();
    void hexColor();
    void hsvRoundtrip_data();
    void hsvRoundtrip();

    // ── 非数据驱动：HSV / HSL ──────────────────────────
    void hsvBasics();
    void hslBasics();
    void hsvFromRgb();
    void hsvSettersWrap();

    // ── 非数据驱动：浮点工厂 ───────────────────────────
    void rgbF();
    void hsvF();
    void hslF();

    // ── 非数据驱动：设定 / 越界 ────────────────────────
    void setChannelClamp();
    void setRgbInvalidates();
    void setRgbaOpaque();
    void setRedOnHsvConvertsToRgb();
    void setAlphaOnExtendedRgb();

    // ── 非数据驱动：派生与命名 ─────────────────────────
    void lighter();
    void darker();
    void name();
    void nameArgb();

    // ── 非数据驱动：比较 ───────────────────────────────
    void equality();
    void equalityAlphaMatters();
    void equalitySpecMatters();

    // ── 非数据驱动：setNamedColor 边界 ─────────────────
    void setNamedColorEdge();
};

#undef Q_SLOTS
#undef Q_OBJECT
