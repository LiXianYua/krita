

/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCURSOROVERRIDELOCK_H
#define KISCURSOROVERRIDELOCK_H

#include <kritaglobal_export.h>

#include <KisAdaptedLock.h>

class KRITAGLOBAL_EXPORT KisCursorOverrideLockAdapter
{
public:
    KisCursorOverrideLockAdapter();
    ~KisCursorOverrideLockAdapter();

    void lock();
    void unlock();
};

// 注意：当前类不可默认构造（KIS_DECLARE_ADAPTED_LOCK 的 using BaseClass::BaseClass
// 不继承默认构造）。全树零消费方（死代码）；消费方接入时需先恢复构造路径
// （给 Adapter 加带参构造 + 相应转发，或给 KisAdaptedLock 补默认构造）。
KIS_DECLARE_ADAPTED_LOCK(KisCursorOverrideLock, KisCursorOverrideLockAdapter)

#endif // KISCURSOROVERRIDELOCK_H
