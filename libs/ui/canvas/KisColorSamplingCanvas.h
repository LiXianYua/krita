/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_COLOR_SAMPLING_CANVAS_H
#define KIS_COLOR_SAMPLING_CANVAS_H

#include <optional>

#include <QColor>
#include <QPoint>

#include <KoColor.h>
#include <kis_types.h>
#include <kritacanvas_export.h>

/**
 * Narrow host services needed by asynchronous canvas color sampling.
 *
 * Node and foreground/background resources remain on KoCanvasBase. This port
 * only covers image/view state that has no generic canvas representation.
 */
class KRITACANVAS_EXPORT KisColorSamplingCanvas
{
public:
    virtual ~KisColorSamplingCanvas();

    virtual KisImageWSP samplingImage() const = 0;
    virtual std::optional<KoColor>
        sampleVisibleReferenceColor(const QPoint &imagePoint) const = 0;
    virtual QColor samplingPreviewColor(const KoColor &color) const = 0;
    virtual qreal samplingCanvasRotation() const = 0;
    virtual bool samplingCanvasMirroredHorizontally() const = 0;
    virtual bool samplingCanvasMirroredVertically() const = 0;
};

#endif // KIS_COLOR_SAMPLING_CANVAS_H
