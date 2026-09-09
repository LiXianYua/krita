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
    void initTestCase();
    void cleanup();
    void delayedPreviewWaitsForPkTimerPump();
    void deactivationCancelsDelayedPreview();
    void destructionInvalidatesQueuedPreview();
    void referenceColorShortCircuitsDeviceSampling();
    void missingReferenceFallsBackToProjection();
    void delayedJobReadsTheCurrentNodeAgain();
    void previewUsesSamplingCanvasGeometry();
    void ellipsePreviewRoundsBeforeRotation();
    void rectanglePreviewPreservesCommandsAndState();
    void circlePreviewPreservesRingCommandsAndState();
    void cursorUsesSamplingCanvasPolicy();
    void proxyDispatchesPolylineAndSelectionDecorations();
    void hostCallbacksPreserveActionAndRightClickLifecycle();
    void hostCallbacksDropDispatchAfterDirectToolDestruction();
    void priorityEventFilterUsesPkIdentity();
    void hostKeyAdapterDispatchesPkPayload();
    void testWorkerThreadSampleDelivery();
    void testWorkerThreadSampleDeliveryAfterHelperDestruction();
    void proxyDispatchesProductionAsyncSampler();
};

#endif // KIS_ASYNC_COLOR_SAMPLER_HELPER_TEST_H
