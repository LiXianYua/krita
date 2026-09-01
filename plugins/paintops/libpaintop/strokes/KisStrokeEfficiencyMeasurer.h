/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSTROKEEFFICIENCYMEASURER_H
#define KISSTROKEEFFICIENCYMEASURER_H

#include <kritapaintop_export.h>
#include <PkGlobal.h>
#include <PkScopedPointer.h>
#include <PkVector.h>

class PkPointF;

class PAINTOP_EXPORT KisStrokeEfficiencyMeasurer
{
public:
    KisStrokeEfficiencyMeasurer();
    ~KisStrokeEfficiencyMeasurer();

    void setEnabled(bool value);
    bool isEnabled() const;

    void addSample(const PkPointF &pt);
    void addSamples(const PkVector<PkPointF> &points);

    qreal averageCursorSpeed() const;
    qreal averageRenderingSpeed() const;
    qreal averageFps() const;

    void notifyRenderingStarted();
    void notifyRenderingFinished();

    void notifyCursorMoveStarted();
    void notifyCursorMoveFinished();

    void notifyFrameRenderingStarted();

    void reset();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISSTROKEEFFICIENCYMEASURER_H
