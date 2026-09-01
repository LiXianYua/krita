/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_MULTIPLE_PROJECTION_H
#define __KIS_MULTIPLE_PROJECTION_H

#include <PkScopedPointer.h>
#include "kis_types.h"
#include "kritaimage_export.h"

class KisLayerStyleFilterEnvironment;
class PkBitArray;
class PkRect;

class KRITAIMAGE_EXPORT KisMultipleProjection
{
public:
    KisMultipleProjection();
    KisMultipleProjection(const KisMultipleProjection &rhs);
    ~KisMultipleProjection();

    static PkString defaultProjectionId();

    KisPaintDeviceSP getProjection(const PkString &id, const PkString &compositeOpId, quint8 opacity, const PkBitArray &channelFlags, KisPaintDeviceSP prototype);
    void freeProjection(const PkString &id);
    void freeAllProjections();

    void clear(const PkRect &rc);

    void apply(KisPaintDeviceSP dstDevice, const PkRect &rect, KisLayerStyleFilterEnvironment *env);

    KisPaintDeviceList getLodCapableDevices() const;

    bool isEmpty() const;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_MULTIPLE_PROJECTION_H */
