/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_ASYNC_COLOR_SAMPLER_HELPER_TEST_H
#define KIS_ASYNC_COLOR_SAMPLER_HELPER_TEST_H

#include <QObject>

class KisAsyncColorSamplerHelperTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void referenceColorShortCircuitsDeviceSampling();
    void missingReferenceFallsBackToProjection();
    void delayedJobReadsTheCurrentNodeAgain();
    void previewUsesSamplingCanvasGeometry();
};

#endif // KIS_ASYNC_COLOR_SAMPLER_HELPER_TEST_H
