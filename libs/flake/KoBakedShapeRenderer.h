/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOBAKEDSHAPERENDERER_H
#define KOBAKEDSHAPERENDERER_H

#include <PkImage.h>
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

        PkTransform relativeToBakedShape;

        if (referenceIsObb || contentIsObb) {
            m_relativeToShape = toQTransform(KisAlgebra2D::mapToRect(toPkRectF(dstShapeBoundingRect)));
            relativeToBakedShape = toQTransform(KisAlgebra2D::mapToRect(toPkRectF(bakedShapeBoundingRect)));
        }


        m_referenceRectUser =
            referenceIsObb ?
            m_relativeToShape.mapRect(referenceRect).toAlignedRect() :
            referenceRect.toAlignedRect();

        m_patch = PkImage(m_referenceRectUser.size(), PkImage::Format_ARGB32);
        m_patch.fill(0);
        m_patchPainter.begin(&m_patch);

        m_patchPainter.translate(-m_referenceRectUser.topLeft());
        m_patchPainter.setClipRect(m_referenceRectUser);

        if (contentIsObb) {
            m_patchPainter.setTransform(m_relativeToShape, true);
            m_patchPainter.setTransform(relativeToBakedShape.inverted(), true);
        }

        m_patchPainter.setTransform(bakedTransform.inverted(), true);
    }

    QPainter* bakeShapePainter() {
        return &m_patchPainter;
    }

    void renderShape(QPainter &painter) {
        painter.save();

        painter.setTransform(m_dstShapeTransform, true);
        painter.setClipPath(m_dstShapeOutline);

        PkTransform brushTransform;

        PkPointF patternOffset = m_referenceRectUser.topLeft();

        brushTransform =
            brushTransform *
            PkTransform::fromTranslate(patternOffset.x(), patternOffset.y());

        if (m_contentIsObb) {
            brushTransform = brushTransform * m_relativeToShape.inverted();
        }

        brushTransform = brushTransform * m_patternTransform;

        if (m_contentIsObb) {
            brushTransform = brushTransform * m_relativeToShape;
        }

        QBrush brush(m_patch);
        brush.setTransform(brushTransform);

        painter.setBrush(brush);
        painter.drawPath(m_dstShapeOutline);

        painter.restore();
    }

    PkImage patchImage() const {
        return m_patch;
    }


private:
    PkPainterPath m_dstShapeOutline;
    PkTransform m_dstShapeTransform;

    bool m_contentIsObb;
    const PkTransform &m_patternTransform;

    PkImage m_patch;
    QPainter m_patchPainter;

    PkTransform m_relativeToShape;
    PkRect m_referenceRectUser;
};

#endif // KOBAKEDSHAPERENDERER_H
