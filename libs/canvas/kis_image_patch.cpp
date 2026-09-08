/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_image_patch.h"

#include "kis_debug.h"

#include <math.h>

/****** Some helper functions *******/

inline void scaleRect(PkRectF &rc, qreal scaleX, qreal scaleY)
{
    qreal x, y, w, h;
    rc.getRect(&x, &y, &w, &h);

    x *= scaleX;
    y *= scaleY;
    w *= scaleX;
    h *= scaleY;

    rc.setRect(x, y, w, h);
}

inline void scaleRect(PkRect &rc, qreal scaleX, qreal scaleY)
{
    qint32 x, y, w, h;
    rc.getRect(&x, &y, &w, &h);

    x *= scaleX;
    y *= scaleY;
    w *= scaleX;
    h *= scaleY;

    rc.setRect(x, y, w, h);
}

/*********** KisImagePatch ************/

KisImagePatch::KisImagePatch()
{
}

KisImagePatch::KisImagePatch(PkRect imageRect, qint32 borderWidth,
                             qreal scaleX, qreal scaleY)
    : m_scaleX(scaleX)
    , m_scaleY(scaleY)
    , m_isScaled(false)
{
    // First we get unscaled rects
    m_interestRect = PkRectF(borderWidth, borderWidth,
                            imageRect.width(), imageRect.height());
    m_patchRect = imageRect.adjusted(-borderWidth, -borderWidth,
                                     borderWidth, borderWidth);
    // And then we scale them
    scaleRect(m_interestRect, scaleX, scaleY);
    scaleRect(m_patchRect, scaleX, scaleY);

    dbgRender << "A new patch has been created:";
    dbgRender << ppVar(scaleX) << ppVar(scaleY);
    dbgRender << ppVar(m_interestRect);
    dbgRender << ppVar(m_patchRect);
}

void KisImagePatch::setImage(PkImage image)
{
    m_image = image;
    m_isScaled = false;
}

void KisImagePatch::preScale(const PkRectF &dstRect)
{
    if (m_isScaled) return;

    qreal scaleX = dstRect.width() / m_interestRect.width();
    qreal scaleY = dstRect.height() / m_interestRect.height();

    PkSize newImageSize = PkSize(ceil(m_image.width() * scaleX),
                                 ceil(m_image.height() * scaleY));
    // Calculating new _aligned_ scale
    scaleX = qreal(newImageSize.width()) / m_image.width();
    scaleY = qreal(newImageSize.height()) / m_image.height();

    m_scaleX *= scaleX;
    m_scaleY *= scaleY;

    scaleRect(m_interestRect, scaleX, scaleY);

    m_image = m_image.scaled(newImageSize, Pk::IgnoreAspectRatio, Pk::SmoothTransformation);

    m_isScaled = true;

}

PkRect KisImagePatch::patchRect()
{
    return m_patchRect;
}

bool KisImagePatch::isValid()
{
    return !m_image.isNull();
}

void KisImagePatch::drawMe(PkPainter &gc,
                           const PkRectF &dstRect,
                           unsigned renderHints)
{
    gc.save();
    gc.setCompositionMode(Pk::CompositionMode_Source);
    gc.setRenderHints(renderHints, true);
    gc.drawImage(dstRect, m_image, m_interestRect);
    gc.restore();

#if 0
    /**
     * Just for debugging purposes
     */
    qreal scaleX = dstRect.width() / m_interestRect.width();
    qreal scaleY = dstRect.height() / m_interestRect.height();
    dbgRender << "## PATCH.DRAWME #####################";
    dbgRender << ppVar(scaleX) << ppVar(scaleY);
    dbgRender << ppVar(m_patchRect);
    dbgRender << ppVar(m_interestRect);
    dbgRender << ppVar(dstRect);
    dbgRender << "## EODM #############################";
#endif
}
