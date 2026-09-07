/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISASYNCCOLORSAMPLERHELPER_H
#define KISASYNCCOLORSAMPLERHELPER_H

#include "kritacanvas_export.h"

#include <QObject>
#include <PkScopedPointer.h>
#include <pk/render/PkPainter.h>

#include "kis_types.h"

class KoCanvasBase;
class KoViewConverter;
class KisStrokesFacade;
class KisColorSamplingCanvas;
class KoColor;

class KRITACANVAS_EXPORT KisAsyncColorSamplerHelper : public QObject
{
    Q_OBJECT
public:
    KisAsyncColorSamplerHelper(KoCanvasBase *canvas,
                               KisColorSamplingCanvas *samplingCanvas);
    ~KisAsyncColorSamplerHelper() override;

    bool isActive() const;

    void activate(bool sampleCurrentLayer, bool pickFgColor);
    void deactivate();

    void startAction(const PkPointF &docPoint, int radius, int blend);
    void continueAction(const PkPointF &docPoint);
    void endAction();

    PkRectF colorPreviewDocRect(const PkPointF &docPoint);
    void paint(PkPainter &gc, const KoViewConverter &converter);

    void updateCursor(bool sampleCurrentLayer, bool pickFgColor);

    void setUpdateGlobalColor(bool value);
    bool updateGlobalColor() const;

Q_SIGNALS:
    void sigRequestUpdateOutline();
    void sigRequestCursor(const QCursor &cursor);
    void sigRequestCursorReset();
    /**
     * Notifies about the raw color picked from the layer,
     * including its alpha channel.
     */
    void sigRawColorSelected(const KoColor &color);

    /**
     * Notifies about the "palette" color picked from the layer,
     * that is, with the alpha channel set to OPACITY_OPAQUE.
     */
    void sigColorSelected(const KoColor &color);

    /**
     * Notifies about the "palette" color picked from the layer,
     * that is, with the alpha channel set to OPACITY_OPAQUE.
     *
     * This notification is emitted only once at the very end
     * of the color picking stroke.
     */
    void sigFinalColorSelected(const KoColor &color);

private Q_SLOTS:
    void activateDelayedPreview();
    void slotAddSamplingJob(const PkPointF &docPoint);
    void slotColorSamplingFinished(const KoColor &rawColor);

private:
    void activatePreview();
    void paintRectangle(PkPainter &gc, const PkRectF &viewRectF, const PkColor &currentColor, const PkColor &baseColor);
    void paintCircle(PkPainter &gc, const PkRectF &viewRectF, const PkColor &currentColor, const PkColor &baseColor);

    struct Private;
    PkScopedPointer<Private> m_d;
};

#endif // KISASYNCCOLORSAMPLERHELPER_H
