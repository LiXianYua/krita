/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoClipMaskPainter.h"
#include <QPainterPath>
#include <PkFlakeBridge.h>

#include <QPainter>
#include <PkPainterPath.h>
#include <PkRect.h>
#include <KoStreamedMath.h>
#include <KoClipMaskApplicatorBase.h>
#include <xsimd/KoClipMaskApplicatorFactoryImpl.h>

#include "kis_assert.h"
// [migrate] missing include for Pk/Qt type
#include <PkImage.h>

struct KoClipMaskApplicatorFactory {
    static KoClipMaskApplicatorBase* createApplicator() {
#ifndef DISABLE_CLIP_MASK_PAINTER_ON_MACOS
        return createOptimizedClass<KoClipMaskApplicatorFactoryImpl>();
#else
        return KoClipMaskApplicatorFactoryImpl::create<xsimd::generic>();
#endif
    }
};

struct Q_DECL_HIDDEN KoClipMaskPainter::Private
{
    QPainter *globalPainter;

    QImage shapeImage;
    QImage maskImage;

    QPainter shapePainter;
    QPainter maskPainter;

    PkRect alignedGlobalClipRect;
};

KoClipMaskPainter::KoClipMaskPainter(QPainter *painter, const PkRectF &globalClipRect)
    : m_d(new Private)
{
    m_d->globalPainter = painter;
    m_d->alignedGlobalClipRect = globalClipRect.toAlignedRect();

    if (!m_d->alignedGlobalClipRect.isValid()) {
        m_d->alignedGlobalClipRect = PkRect();
    }
    m_d->shapeImage = QImage(toQSize(m_d->alignedGlobalClipRect.size()), QImage::Format_ARGB32);
    m_d->maskImage = QImage(toQSize(m_d->alignedGlobalClipRect.size()), QImage::Format_ARGB32);

    PkTransform moveToBufferTransform =
        PkTransform::fromTranslate(-m_d->alignedGlobalClipRect.x(),
                                  -m_d->alignedGlobalClipRect.y());

    m_d->shapePainter.begin(&m_d->shapeImage);

    m_d->shapePainter.save();
    m_d->shapePainter.setCompositionMode(QPainter::CompositionMode_Source);
    m_d->shapePainter.fillRect(toQRect(PkRect(PkPoint(), m_d->alignedGlobalClipRect.size())), Qt::transparent);
    m_d->shapePainter.restore();

    m_d->shapePainter.setTransform(toQTransform(moveToBufferTransform));
    m_d->shapePainter.setTransform(painter->transform(), true);
    if (painter->hasClipping()) {
        m_d->shapePainter.setClipPath(painter->clipPath());
    }
    m_d->shapePainter.setOpacity(painter->opacity());
    m_d->shapePainter.setBrush(painter->brush());
    m_d->shapePainter.setPen(painter->pen());

    m_d->maskPainter.begin(&m_d->maskImage);

    m_d->maskPainter.save();
    m_d->maskPainter.setCompositionMode(QPainter::CompositionMode_Source);
    m_d->maskPainter.fillRect(toQRect(PkRect(PkPoint(), m_d->alignedGlobalClipRect.size())), Qt::transparent);
    m_d->maskPainter.restore();

    m_d->maskPainter.setTransform(toQTransform(moveToBufferTransform));
    m_d->maskPainter.setTransform(painter->transform(), true);
    if (painter->hasClipping()) {
        m_d->maskPainter.setClipPath(painter->clipPath());
    }
    m_d->maskPainter.setOpacity(painter->opacity());
    m_d->maskPainter.setBrush(painter->brush());
    m_d->maskPainter.setPen(painter->pen());



}

KoClipMaskPainter::~KoClipMaskPainter()
{
}

QPainter *KoClipMaskPainter::shapePainter()
{
    return &m_d->shapePainter;
}

QPainter *KoClipMaskPainter::maskPainter()
{
    return &m_d->maskPainter;
}

void KoClipMaskPainter::renderOnGlobalPainter()
{
    KIS_ASSERT_RECOVER_RETURN(m_d->maskImage.size() == m_d->shapeImage.size());

    const int nPixels = m_d->maskImage.height() * m_d->maskImage.width();

    KoClipMaskApplicatorBase *applicator = KoClipMaskApplicatorFactory::createApplicator();
    applicator->applyLuminanceMask(m_d->shapeImage.bits(),
                                  m_d->maskImage.bits(),
                                  nPixels);

    KIS_ASSERT_RECOVER_RETURN(m_d->shapeImage.size() == toQSize(m_d->alignedGlobalClipRect.size()));
    QPainterPath globalClipPath;

    if (m_d->globalPainter->hasClipping()) {
        globalClipPath = m_d->globalPainter->transform().map(m_d->globalPainter->clipPath());
    }

    m_d->globalPainter->save();

    m_d->globalPainter->setTransform(toQTransform(PkTransform()));

    if (!globalClipPath.isEmpty()) {
        m_d->globalPainter->setClipPath(globalClipPath);
    }

    const PkPoint tp = m_d->alignedGlobalClipRect.topLeft();
        m_d->globalPainter->drawImage(QPoint(tp.x(), tp.y()), m_d->shapeImage);
    m_d->globalPainter->restore();
}
