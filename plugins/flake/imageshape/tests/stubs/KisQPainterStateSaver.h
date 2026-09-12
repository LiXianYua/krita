/* SPDX-License-Identifier: GPL-2.0-or-later */
#pragma once

// R-74：真实现见 libs/flake/KisQPainterStateSaver.{h,cpp} —— 一个包 PkPainter
// save/restore 的 RAII（ctor save、dtor restore），已是 pk 契约，不含 Qt。
// 本 target 不链 kritaflake（见 tests/CMakeLists.txt 的 include 说明），该头不在
// 其 include 面里，故在此给同语义的任务局部定义——行为与真实现逐字相同。
#include <PkPainter.h>

class KisQPainterStateSaver
{
public:
    explicit KisQPainterStateSaver(PkPainter *painter)
        : m_painter(painter)
    {
        m_painter->save();
    }
    ~KisQPainterStateSaver()
    {
        m_painter->restore();
    }

private:
    KisQPainterStateSaver(const KisQPainterStateSaver &) = delete;
    PkPainter *m_painter;
};
