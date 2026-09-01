/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISMASKINGBRUSHRENDERER_H
#define KISMASKINGBRUSHRENDERER_H

#include <PkRect.h>
#include <PkScopedPointer.h>

#include "kis_types.h"

class KisMaskingBrushCompositeOpBase;


class KisMaskingBrushRenderer
{
public:
    KisMaskingBrushRenderer(KisPaintDeviceSP dstDevice, const PkString &compositeOpId);
    ~KisMaskingBrushRenderer();

    KisPaintDeviceSP strokeDevice() const;
    KisPaintDeviceSP maskDevice() const;

    void updateProjection(const PkRect &rc);


private:
    KisPaintDeviceSP m_strokeDevice;
    KisPaintDeviceSP m_maskDevice;
    KisPaintDeviceSP m_dstDevice;

    PkScopedPointer<KisMaskingBrushCompositeOpBase> m_compositeOp;
};

#endif // KISMASKINGBRUSHRENDERER_H
