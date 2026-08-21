/*
 * SPDX-FileCopyrightText: 2021 Mathias Wein <lynx.mw+kde@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KISUNIQUECOLORSET_H
#define KISUNIQUECOLORSET_H


#include "kritapigment_export.h"

#include "KoColor.h"

#include <PkObject.h>
#include <memory>

class KRITAPIGMENT_EXPORT KisUniqueColorSet : public PkObject
{
    // Task 8 处理 Qt 元对象系统（moc）：此文件原为 Qt Object 基类 + Q_OBJECT +
    // Q_SIGNALS + Q_SLOTS（moc 文件）。本 Task 只剥类型（Object→PkObject）
    // 并处理 .cpp 的 emit 调用点，故这里以普通成员函数形式保留原信号/slot
    // 声明，访问控制按
    // Q_SIGNALS/Q_SLOTS 的展开（public）写死；Task 8 改声明为 PkSignal 形态并
    // 提供定义（pk_signal_moc.py）。.cpp 里对 sig* 的直接调用在薄壳 .so 中
    // 为未定义函数符号（薄壳允许），定义归 Task 8。
public:
    explicit KisUniqueColorSet(PkObject *parent = nullptr);
    ~KisUniqueColorSet() override;

    void addColor(const KoColor &color);
    KoColor color(int index) const;
    int size() const;

public: // 原 Q_SLOTS
    void clear();

public: // 原 Q_SIGNALS（Task 8 改为 PkSignal 声明 + pk_signal_moc 生成定义）
    void sigReset();
    void sigColorAdded(int position);
    void sigColorMoved(int from, int to);
    void sigColorRemoved(int position);
private:
    struct ColorEntry;
    struct Private;
    std::unique_ptr<Private> d;
};

#endif // KISUNIQUECOLORSET_H
