/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOBAKEDSHAPERENDERER_H
#define KOBAKEDSHAPERENDERER_H

#include <PkImage.h>
#include <QImage>
#include <PkFlakeBridge.h>
#include <QPainter>
#include <PkPainterPath.h>
#include <PkTransform.h>

#include <kis_debug.h>
#include <kis_algebra_2d.h>


struct KoBakedShapeRenderer {
    KoBakedShapeRenderer(const PkPainterPath &dstShapeOutline, const PkTransform &dstShapeTransform,
                       const PkTransform &bakedTransform,
                       const PkRectF &referenceRect,
                       bool contentIsObb, const PkRectF &bakedShapeBoundingRect,
                       bool referenceIsObb,
                       const PkTransform &patternTransform)
        : m_dstShapeOutline(dstShapeOutline),
          m_dstShapeTransform(dstShapeTransform),
          m_contentIsObb(contentIsObb),
          m_patternTransform(patternTransform)
    {
        KIS_SAFE_ASSERT_RECOVER_NOOP(!contentIsObb || !bakedShapeBoundingRect.isEmpty());

        const PkRectF dstShapeBoundingRect = dstShapeOutline.boundingRect();

        QTransform relativeToBakedShape;

        if (referenceIsObb || contentIsObb) {
            m_relativeToShape = toQTransform(KisAlgebra2D::mapToRect(dstShapeBoundingRect));
            relativeToBakedShape = toQTransform(KisAlgebra2D::mapToRect(bakedShapeBoundingRect));
        }


        m_referenceRectUser =
            referenceIsObb ?
            m_relativeToShape.mapRect(toQRectF(referenceRect)).toAlignedRect() :
            toQRect(referenceRect.toAlignedRect());

        m_patch = QImage(m_referenceRectUser.size(), QImage::Format_ARGB32);
        m_patch.fill(0);
        m_patchPainter.begin(&m_patch);

        m_patchPainter.translate(-m_referenceRectUser.topLeft());
        m_patchPainter.setClipRect(m_referenceRectUser);

        if (contentIsObb) {
            m_patchPainter.setTransform(m_relativeToShape, true);
            m_patchPainter.setTransform(relativeToBakedShape.inverted(), true);
        }

        m_patchPainter.setTransform(toQTransform(bakedTransform).inverted(), true);
    }

    QPainter* bakeShapePainter() {
        return &m_patchPainter;
    }

    void renderShape(QPainter &painter) {
        painter.save();

        painter.setTransform(toQTransform(m_dstShapeTransform), true);
        painter.setClipPath(toQPainterPath(m_dstShapeOutline));

        QTransform brushTransform;

        const QPoint patternOffset = m_referenceRectUser.topLeft();

        brushTransform =
            brushTransform *
            QTransform::fromTranslate(patternOffset.x(), patternOffset.y());

        if (m_contentIsObb) {
            brushTransform = brushTransform * m_relativeToShape.inverted();
        }

        brushTransform = brushTransform * toQTransform(m_patternTransform);

        if (m_contentIsObb) {
            brushTransform = brushTransform * m_relativeToShape;
        }

        QBrush brush(m_patch);
        brush.setTransform(brushTransform);

        painter.setBrush(brush);
        painter.drawPath(toQPainterPath(m_dstShapeOutline));

        painter.restore();
    }

    QImage patchImage() const {
        return m_patch;
    }


private:
    PkPainterPath m_dstShapeOutline;
    PkTransform m_dstShapeTransform;

    bool m_contentIsObb;
    const PkTransform &m_patternTransform;

    QImage m_patch;
    QPainter m_patchPainter;

    QTransform m_relativeToShape;
    QRect m_referenceRectUser;
};

#endif // KOBAKEDSHAPERENDERER_H
