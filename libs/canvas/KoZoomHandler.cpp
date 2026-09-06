/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2001-2005 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2010 KO GmbH <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <pk/geometry/PkPoint.h>
#include <pk/geometry/PkRect.h>
#include <pk/geometry/PkSize.h>

#include "KoZoomHandler.h"
#include <KoUnit.h> // for POINT_TO_INCH

KoZoomHandler::KoZoomHandler()
    : KoViewConverter()
    , m_zoomMode(KoZoomMode::ZOOM_CONSTANT)
    , m_resolutionX(0)
    , m_resolutionY(0)
    , m_zoomedResolutionX(0)
    , m_zoomedResolutionY(0)
    , m_zoomMarginSize(0)
{
    setZoom(1.0);
    setZoomMode( KoZoomMode::ZOOM_CONSTANT );
    // Use 72 dpi as a placeholder. KisView will immediately update the
    // screen resolution correctly after this, using the values initialized
    // by KisZoomManager::updateScreenResolution().
    setDpi(72, 72);
}

KoZoomHandler::~KoZoomHandler()
{
}

void KoZoomHandler::setDpi(int dpiX, int dpiY)
{
    setResolution(POINT_TO_INCH(static_cast<qreal>(dpiX)),
                  POINT_TO_INCH(static_cast<qreal>(dpiY)));
}

void KoZoomHandler::setResolution( qreal resolutionX, qreal resolutionY )
{

    m_resolutionX = resolutionX;
    m_resolutionY = resolutionY;

    if (pkQtFuzzyCompare(m_resolutionX, 1))
        m_resolutionX = 1;
    if (pkQtFuzzyCompare(m_resolutionY, 1))
        m_resolutionY = 1;

    m_zoomedResolutionX = zoom() * resolutionX;
    m_zoomedResolutionY = zoom() * resolutionY;
}

void KoZoomHandler::setZoomedResolution( qreal zoomedResolutionX, qreal zoomedResolutionY )
{
    // zoom() doesn't matter, it's only used in setZoom() to calculated the zoomed resolutions
    // Here we know them. The whole point of this method is to allow a different zoom factor
    // for X and for Y, as can be useful for e.g. fullscreen kpresenter presentations.
    m_zoomedResolutionX = zoomedResolutionX;
    m_zoomedResolutionY = zoomedResolutionY;
}

void KoZoomHandler::setZoom( qreal zoom )
{
    if (pkQtFuzzyCompare(zoom, qreal(1.0))) {
        zoom = 1.0;
    }

    KoViewConverter::setZoom(zoom);
    if( zoom == 1.0 ) {
        m_zoomedResolutionX = m_resolutionX;
        m_zoomedResolutionY = m_resolutionY;
    } else {
        m_zoomedResolutionX = zoom * m_resolutionX;
        m_zoomedResolutionY = zoom * m_resolutionY;
    }
}

void KoZoomHandler::setZoomMarginSize( int size )
{
    m_zoomMarginSize = size;
}

int KoZoomHandler::zoomMarginSize() const
{
    return m_zoomMarginSize;
}

PkPointF KoZoomHandler::documentToView( const PkPointF &documentPoint )  const
{
    return PkPointF( zoomItX( documentPoint.x() ),
                    zoomItY( documentPoint.y() ));
}

PkPointF KoZoomHandler::viewToDocument( const PkPointF &viewPoint )  const
{
    return PkPointF( unzoomItX( viewPoint.x() ),
                    unzoomItY( viewPoint.y() ) );
}

PkRectF KoZoomHandler::documentToView( const PkRectF &documentRect )  const
{
    PkRectF r (zoomItX( documentRect.x() ),
              zoomItY( documentRect.y() ),
              zoomItX( documentRect.width() ),
              zoomItY( documentRect.height() ) );
    return r;
}

PkRectF KoZoomHandler::viewToDocument( const PkRectF &viewRect )  const
{
    PkRectF r (  unzoomItX( viewRect.x() ),
                unzoomItY( viewRect.y()),
                unzoomItX( viewRect.width() ),
                unzoomItY( viewRect.height() ) );
    return r;
}

PkSizeF KoZoomHandler::documentToView( const PkSizeF &documentSize ) const
{
    return PkSizeF( zoomItX( documentSize.width() ),
                   zoomItY( documentSize.height() ) );
}

PkSizeF KoZoomHandler::viewToDocument( const PkSizeF &viewSize ) const
{
    return PkSizeF( unzoomItX( viewSize.width() ),
                   unzoomItY( viewSize.height() ) );
}

qreal KoZoomHandler::documentToViewX( qreal documentX ) const
{
    return zoomItX( documentX );
}

qreal KoZoomHandler::documentToViewY( qreal documentY ) const
{
    return zoomItY( documentY );
}

qreal KoZoomHandler::viewToDocumentX( qreal viewX ) const
{
    return unzoomItX( viewX );
}

qreal KoZoomHandler::viewToDocumentY( qreal viewY ) const
{
    return unzoomItY( viewY );
}

void KoZoomHandler::zoom(qreal *zoomX, qreal *zoomY) const
{
    *zoomX = zoomItX(100.0) / 100.0;
    *zoomY = zoomItY(100.0) / 100.0;
}
