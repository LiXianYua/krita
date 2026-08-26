/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SPEED_SMOOTHER_H
#define __KIS_SPEED_SMOOTHER_H

#include <PkScopedPointer.h>
#include <kritacanvas_export.h>

class PkPointF;


class KRITACANVAS_EXPORT KisSpeedSmoother
{
public:
    KisSpeedSmoother();
    ~KisSpeedSmoother();

    qreal lastSpeed() const;
    qreal getNextSpeed(const PkPointF &pt, ulong timestamp);
    void clear();

    void updateSettings();

private:
    qreal getNextSpeedImpl(const PkPointF &pt, qreal time);
private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_SPEED_SMOOTHER_H */
