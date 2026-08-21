/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSTROKESPEEDMEASURER_H
#define KISSTROKESPEEDMEASURER_H

#include "kritaimage_export.h"
#include <PkScopedPointer.h>
#include <PkVector.h>


class PkPointF;


class KRITAIMAGE_EXPORT KisStrokeSpeedMeasurer
{
public:
    KisStrokeSpeedMeasurer(int timeSmoothWindow);
    ~KisStrokeSpeedMeasurer();

    void addSample(const PkPointF &pt, int time);
    void addSamples(const PkVector<PkPointF> &points, int time);

    qreal averageSpeed() const;
    qreal currentSpeed() const;
    qreal maxSpeed() const;

    void reset();

private:
    void sampleMaxSpeed();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISSTROKESPEEDMEASURER_H
