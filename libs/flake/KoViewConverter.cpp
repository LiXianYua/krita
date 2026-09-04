/*
 * SPDX-FileCopyrightText: 2006, 2008-2009 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoViewConverter.h"
#include "KoViewTransformStillPoint.h"

#include <PkPoint.h>
#include <PkRect.h>
#include <PkTransform.h>

KoViewConverter::KoViewConverter()
    : m_zoomLevel(1.0)
{
}

PkPointF KoViewConverter::documentToView(const PkPointF &documentPoint) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return documentPoint;
    return PkPointF(documentToViewX(documentPoint.x()), documentToViewY(documentPoint.y()));
}

PkPointF KoViewConverter::viewToDocument(const PkPointF &viewPoint) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return viewPoint;
    return PkPointF(viewToDocumentX(viewPoint.x()), viewToDocumentY(viewPoint.y()));
}

PkRectF KoViewConverter::documentToView(const PkRectF &documentRect) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return documentRect;
    return PkRectF(documentToView(documentRect.topLeft()), documentToView(documentRect.size()));
}

PkRectF KoViewConverter::viewToDocument(const PkRectF &viewRect) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return viewRect;
    return PkRectF(viewToDocument(viewRect.topLeft()), viewToDocument(viewRect.size()));
}

PkSizeF KoViewConverter::documentToView(const PkSizeF &documentSize) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return documentSize;
    return PkSizeF(documentToViewX(documentSize.width()), documentToViewY(documentSize.height()));
}

PkSizeF KoViewConverter::viewToDocument(const PkSizeF &viewSize) const
{
    if (qFuzzyCompare(m_zoomLevel, 1))
        return viewSize;
    return PkSizeF(viewToDocumentX(viewSize.width()), viewToDocumentY(viewSize.height()));
}

void KoViewConverter::zoom(qreal *zoomX, qreal *zoomY) const
{
    *zoomX = m_zoomLevel;
    *zoomY = m_zoomLevel;
}

qreal KoViewConverter::documentToViewX(qreal documentX) const
{
    return documentX * m_zoomLevel;
}

qreal KoViewConverter::documentToViewY(qreal documentY) const
{
    return documentY * m_zoomLevel;
}

qreal KoViewConverter::viewToDocumentX(qreal viewX) const
{
    return viewX / m_zoomLevel;
}

qreal KoViewConverter::viewToDocumentY(qreal viewY) const
{
    return viewY / m_zoomLevel;
}



void KoViewConverter::setZoom(qreal zoom)
{
    if (qFuzzyCompare(zoom, qreal(0.0)) || qFuzzyCompare(zoom, qreal(1.0))) {
        zoom = 1;
    }
    m_zoomLevel = zoom;
}

qreal KoViewConverter::zoom() const
{
    return m_zoomLevel;
}

KoViewTransformStillPoint KoViewConverter::makeWidgetStillPoint(const PkPointF &viewPoint) const
{
    return { viewToDocument(widgetToView().map(viewPoint)), viewPoint };
}

KoViewTransformStillPoint KoViewConverter::makeDocStillPoint(const PkPointF &docPoint) const
{
    return {docPoint, viewToWidget().map(documentToView(docPoint))};
}

PkTransform KoViewConverter::documentToView() const
{
    qreal zoomX, zoomY;
    zoom(&zoomX, &zoomY);
    return PkTransform::fromScale(zoomX, zoomY);
}

PkTransform KoViewConverter::viewToDocument() const
{
    qreal zoomX, zoomY;
    zoom(&zoomX, &zoomY);
    return PkTransform::fromScale(1.0 / zoomX, 1.0 / zoomY);
}

PkTransform KoViewConverter::viewToWidget() const
{
    return PkTransform();
}

PkTransform KoViewConverter::widgetToView() const
{
    return PkTransform();
}
