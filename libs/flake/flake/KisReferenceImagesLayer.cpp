/*
 * SPDX-FileCopyrightText: 2017 Jouni Pentikäinen <joupent@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <kis_node_visitor.h>
#include <kis_processing_visitor.h>
#include <kis_shape_layer_canvas.h>

#include "kis_default_bounds.h"
#include "KisReferenceImagesLayer.h"
#include "KisReferenceImage.h"
#include <KoViewConverter.h>

class ReferenceImagesCanvas : public KisShapeLayerCanvasBase
{
    Q_OBJECT
public:
    ReferenceImagesCanvas(const KoColorSpace *cs, KisDefaultBoundsBaseSP defaultBounds, KisReferenceImagesLayer *parent)
        : KisShapeLayerCanvasBase(parent)
        , m_layer(parent)
        , m_fallbackProjection(new KisPaintDevice(parent, cs, defaultBounds))
        , m_compressor(KisThreadSafeSignalCompressor(25, KisSignalCompressor::FIRST_ACTIVE))
    {
        PkObject::connect(&m_compressor, &KisThreadSafeSignalCompressor::timeout,
                          &m_compressor, [this]() { slotAsyncRepaint(); });
    }

    ReferenceImagesCanvas(const ReferenceImagesCanvas &rhs, KisReferenceImagesLayer *parent)
        : KisShapeLayerCanvasBase(rhs, parent)
        , m_layer(parent)
        , m_fallbackProjection(new KisPaintDevice(*rhs.m_fallbackProjection))
        , m_compressor(KisThreadSafeSignalCompressor(25, KisSignalCompressor::FIRST_ACTIVE))
    {
        PkObject::connect(&m_compressor, &KisThreadSafeSignalCompressor::timeout,
                          &m_compressor, [this]() { slotAsyncRepaint(); });
    }

    void updateCanvas(const QRectF &rect) override
    {
        if (!m_layer->image() || m_isDestroying) {
            return;
        }

        m_dirtyRect |= rect;

        m_compressor.start();
        m_hasUpdateInCompressor = true;
    }

    void forceRepaint() override
    {
        m_layer->signalUpdate(m_layer->boundingImageRect());
    }

    bool hasPendingUpdates() const override
    {
        return m_hasUpdateInCompressor;
    }

    void rerenderAfterBeingInvisible() override {}

    void resetCache(const KoColorSpace *colorSpace) override { 
        Q_UNUSED(colorSpace); 
    }

    KisPaintDeviceSP projection() const override {
        return m_fallbackProjection;
    }
private Q_SLOTS:
    void slotAsyncRepaint() {
        QRectF r = viewConverter()->documentToView(m_dirtyRect);
        m_layer->signalUpdate(r);
        m_dirtyRect = QRectF();
        m_hasUpdateInCompressor = false;
    }

private:
    KisReferenceImagesLayer *m_layer;
    KisPaintDeviceSP m_fallbackProjection;
    KisThreadSafeSignalCompressor m_compressor;
    QRectF m_dirtyRect;
    volatile bool m_hasUpdateInCompressor = false;
};

KisReferenceImagesLayer::KisReferenceImagesLayer(KoShapeControllerBase* shapeController, KisImageWSP image)
    : KisShapeLayer(shapeController, image, i18n("Reference images"), OPACITY_OPAQUE_U8,
                    [&] () { return new ReferenceImagesCanvas(image->colorSpace(), new KisDefaultBounds(image), this); })
{}

KisReferenceImagesLayer::KisReferenceImagesLayer(const KisReferenceImagesLayer &rhs)
    : KisShapeLayer(rhs, rhs.shapeController(),
                    [&] () {
                            const ReferenceImagesCanvas* referenceImagesCanvas = dynamic_cast<const ReferenceImagesCanvas*>(rhs.canvas());
                            KIS_ASSERT(referenceImagesCanvas);
                            return new ReferenceImagesCanvas(*referenceImagesCanvas, this); })
{}

QVector<KisReferenceImage*> KisReferenceImagesLayer::referenceImages() const
{
    QVector<KisReferenceImage*> references;

    Q_FOREACH(auto shape, shapes()) {
        KisReferenceImage *referenceImage = dynamic_cast<KisReferenceImage*>(shape);
        if (referenceImage) {
            references.append(referenceImage);
        }
    }
    return references;
}

void KisReferenceImagesLayer::paintReferences(QPainter &painter) {
    painter.setTransform(converter()->documentToView(), true);
    shapeManager()->paint(painter);
}

bool KisReferenceImagesLayer::allowAsChild(KisNodeSP) const
{
    return false;
}

bool KisReferenceImagesLayer::accept(KisNodeVisitor &visitor)
{
    return visitor.visit(this);
}

void KisReferenceImagesLayer::accept(KisProcessingVisitor &visitor, KisUndoAdapter *undoAdapter)
{
    visitor.visit(this, undoAdapter);
}

bool KisReferenceImagesLayer::isFakeNode() const
{
    return true;
}

KUndo2Command *KisReferenceImagesLayer::setProfile(const KoColorProfile *profile)
{
    // references should not be converted with the image
    Q_UNUSED(profile);
    return 0;
}

KUndo2Command *KisReferenceImagesLayer::convertTo(const KoColorSpace *dstColorSpace, KoColorConversionTransformation::Intent renderingIntent, KoColorConversionTransformation::ConversionFlags conversionFlags)
{
    // references should not be converted with the image
    Q_UNUSED(dstColorSpace);
    Q_UNUSED(renderingIntent);
    Q_UNUSED(conversionFlags);
    return 0;
}

void KisReferenceImagesLayer::signalUpdate(const QRectF &rect)
{
    Q_EMIT sigUpdateCanvas(rect);
}

QRectF KisReferenceImagesLayer::boundingImageRect() const
{
    return converter()->documentToView(boundingRect());
}

QColor KisReferenceImagesLayer::getPixel(QPointF position) const
{
    const QPointF docPoint = converter()->viewToDocument(position);

    KoShape *shape = shapeManager()->shapeAt(docPoint);

    if (shape) {
        auto *reference = dynamic_cast<KisReferenceImage*>(shape);
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(reference, QColor());

        return reference->getPixel(docPoint);
    }

    return QColor();
}

#include "KisReferenceImagesLayer.moc"
