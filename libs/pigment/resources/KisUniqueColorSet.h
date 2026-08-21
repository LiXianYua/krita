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
#include <PkSignalCompat.h>
#include <memory>

class KRITAPIGMENT_EXPORT KisUniqueColorSet : public PkObject
{
    // 信号/slot 声明采用 PkSignal 形态（pk/signal）：信号段用 signals 标记声明，
    // 定义由 pk/signal/pk_signal_moc.py 生成（体内调用 activateSignal）；
    // clear() 保持原 moc 的 public 访问控制。.cpp 里对 sig* 的直接调用即发射。
public:
    explicit KisUniqueColorSet(PkObject *parent = nullptr);
    ~KisUniqueColorSet() override;

    void addColor(const KoColor &color);
    KoColor color(int index) const;
    int size() const;

public: // clear 是原 public slot，保持 public 访问控制
    void clear();

signals:
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
