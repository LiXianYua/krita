/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QObject>
#include <QPointF>
#include <QVariant>
#include <QVector>

#include "kis_painting_information_builder.h"

#include <KoPointerEvent.h>
#include <KoCanvasBase.h>

#include <kis_image_config.h>
#include "kis_config_notifier.h"

#include "kis_cubic_curve.h"
#include "kis_speed_smoother.h"
#include <PkPoint.h>

#include <KoCanvasResourceProvider.h>
#include <PkObject.h>


/***********************************************************************/
/*           KisPaintingInformationBuilder                             */
/***********************************************************************/


const int KisPaintingInformationBuilder::LEVEL_OF_PRESSURE_RESOLUTION = 1024;


KisPaintingInformationBuilder::KisPaintingInformationBuilder()
    : m_speedSmoother(new KisSpeedSmoother()),
      m_pressureDisabled(false)
{
    KisConfigNotifier *notifier = KisConfigNotifier::instance();
    PkConnection configConnection = PkObject::connect(
        notifier, &KisConfigNotifier::configChanged, notifier,
        [this]() { updateSettings(); });
    QObject::connect(this, &QObject::destroyed,
                     [configConnection](QObject *) mutable {
                         PkObject::disconnect(configConnection);
                     });

    updateSettings();
}

KisPaintingInformationBuilder::~KisPaintingInformationBuilder()
{

}

void KisPaintingInformationBuilder::updateSettings()
{
    KisImageConfig cfg(true);
    const KisCubicCurve curve(cfg.pressureTabletCurve());
    m_pressureSamples.clear();
    const PkVector<qreal> pressureSamples = curve.floatTransfer(LEVEL_OF_PRESSURE_RESOLUTION + 1);
    m_pressureSamples.reserve(pressureSamples.size());
    for (qreal pressure : pressureSamples) {
        m_pressureSamples.append(pressure);
    }
    m_maxAllowedSpeedValue = cfg.readEntry("maxAllowedSpeedValue", 30);

    // The setting is stored in [-180, 180] degrees range to match the UI.
    // We now need to convert it to [0, 360) range.
    m_tiltDirectionOffset = cfg.readEntry("tiltDirectionOffset", 0);
    if (m_tiltDirectionOffset < 0.0) {
        m_tiltDirectionOffset += 360.0;
    }

    m_speedSmoother->updateSettings();
}

KisPaintInformation KisPaintingInformationBuilder::startStroke(KoPointerEvent *event,
                                                               int timeElapsed,
                                                               const KoCanvasResourceProvider *manager)
{
    if (manager) {
        m_pressureDisabled = manager->resource(KoCanvasResource::DisablePressure).toBool();
    }

    m_startPoint = event->point;
    return createPaintingInformation(event, timeElapsed);

}

KisPaintInformation KisPaintingInformationBuilder::continueStroke(KoPointerEvent *event,
                                                                  int timeElapsed)
{
    return createPaintingInformation(event, timeElapsed);
}

QPointF KisPaintingInformationBuilder::adjustDocumentPoint(const QPointF &point, const QPointF &/*startPoint*/)
{
    return point;
}

QPointF KisPaintingInformationBuilder::documentToImage(const QPointF &point)
{
    return point;
}

QPointF KisPaintingInformationBuilder::imageToDocument(const QPointF &point)
{
    return point;
}

QPointF KisPaintingInformationBuilder::imageToView(const QPointF &point)
{
    return point;
}

qreal KisPaintingInformationBuilder::calculatePerspective(const QPointF &documentPoint)
{
    Q_UNUSED(documentPoint);
    return 1.0;
}

qreal KisPaintingInformationBuilder::canvasRotation() const
{
    return 0;
}

bool KisPaintingInformationBuilder::canvasMirroredX() const
{
    return false;
}

bool KisPaintingInformationBuilder::canvasMirroredY() const
{
    return false;
}

KisPaintInformation KisPaintingInformationBuilder::createPaintingInformation(KoPointerEvent *event,
                                                                             int timeElapsed)
{

    QPointF adjusted = adjustDocumentPoint(event->point, m_startPoint);
    QPointF imagePoint = documentToImage(adjusted);
    qreal perspective = calculatePerspective(adjusted);
    const QPointF viewPoint = imageToView(imagePoint);
    const qreal speed = m_speedSmoother->getNextSpeed(PkPointF(viewPoint.x(), viewPoint.y()), event->time());

    KisPaintInformation pi(PkPointF(imagePoint.x(), imagePoint.y()),
                           !m_pressureDisabled ? 1.0 : pressureToCurve(event->pressure()),
                           event->xTilt(), event->yTilt(),
                           event->rotation(),
                           event->tangentialPressure(),
                           perspective,
                           timeElapsed,
                           qMin(1.0, speed / qreal(m_maxAllowedSpeedValue)));

    pi.setCanvasRotation(canvasRotation());
    pi.setCanvasMirroredH(canvasMirroredX());
    pi.setCanvasMirroredV(canvasMirroredY());
    pi.setTiltDirectionOffset(m_tiltDirectionOffset);

    return pi;
}

KisPaintInformation KisPaintingInformationBuilder::hover(const QPointF &imagePoint,
                                                         const KoPointerEvent *event,
                                                         bool isStrokeStarted)
{
    const qreal perspective = calculatePerspective(imageToDocument(imagePoint));

    qreal speed;
    if (!isStrokeStarted && event) {
        const QPointF viewPoint = imageToView(imagePoint);
        speed = m_speedSmoother->getNextSpeed(PkPointF(viewPoint.x(), viewPoint.y()), event->time());
    } else {
        speed = m_speedSmoother->lastSpeed();
    }

    if (event) {
        return KisPaintInformation::createHoveringModeInfo(PkPointF(imagePoint.x(), imagePoint.y()),
                                                           PRESSURE_DEFAULT,
                                                           event->xTilt(), event->yTilt(),
                                                           event->rotation(),
                                                           event->tangentialPressure(),
                                                           perspective,
                                                           qMin(1.0, speed / qreal(m_maxAllowedSpeedValue)),
                                                           canvasRotation(),
                                                           canvasMirroredX(),
                                                           canvasMirroredY(),
                                                           m_tiltDirectionOffset);
    } else {
        KisPaintInformation pi = KisPaintInformation::createHoveringModeInfo(PkPointF(imagePoint.x(), imagePoint.y()));
        pi.setCanvasRotation(canvasRotation());
        pi.setCanvasMirroredH(canvasMirroredX());
        pi.setCanvasMirroredV(canvasMirroredY());
        pi.setTiltDirectionOffset(m_tiltDirectionOffset);
        return pi;
    }
}

qreal KisPaintingInformationBuilder::pressureToCurve(qreal pressure)
{
    PkVector<qreal> pressureSamples;
    pressureSamples.reserve(m_pressureSamples.size());
    for (qreal sample : m_pressureSamples) {
        pressureSamples.append(sample);
    }
    return KisCubicCurve::interpolateLinear(pressure, pressureSamples);
}

void KisPaintingInformationBuilder::reset()
{
    m_speedSmoother->clear();
}

/***********************************************************************/
/*           KisConverterPaintingInformationBuilder                        */
/***********************************************************************/

#include "kis_coordinates_converter.h"

KisConverterPaintingInformationBuilder::KisConverterPaintingInformationBuilder(const KisCoordinatesConverter *converter)
    : m_converter(converter)
{
}

QPointF KisConverterPaintingInformationBuilder::documentToImage(const QPointF &point)
{
    return m_converter->documentToImage(point);
}

QPointF KisConverterPaintingInformationBuilder::imageToDocument(const QPointF &point)
{
    return m_converter->imageToDocument(point);
}

QPointF KisConverterPaintingInformationBuilder::imageToView(const QPointF &point)
{
    return m_converter->imageToWidget(point);
}

qreal KisConverterPaintingInformationBuilder::canvasRotation() const
{
    return m_converter->rotationAngle();
}

bool KisConverterPaintingInformationBuilder::canvasMirroredX() const
{
    return m_converter->xAxisMirrored();
}

bool KisConverterPaintingInformationBuilder::canvasMirroredY() const
{
    return m_converter->yAxisMirrored();
}

/***********************************************************************/
/*           KisToolFreehandPaintingInformationBuilder                 */
/***********************************************************************/

#include "kis_tool_freehand.h"
#include <KisCanvasToolServices.h>

KisToolFreehandPaintingInformationBuilder::KisToolFreehandPaintingInformationBuilder(KisToolFreehand *tool)
    : m_tool(tool)
{
}

QPointF KisToolFreehandPaintingInformationBuilder::documentToImage(const QPointF &point)
{
    return m_tool->convertToPixelCoord(point);
}

QPointF KisToolFreehandPaintingInformationBuilder::imageToDocument(const QPointF &point)
{
    KisCanvasToolServices *canvas = dynamic_cast<KisCanvasToolServices*>(m_tool->canvas());
    KIS_ASSERT_RECOVER_RETURN_VALUE(canvas, point);
    return canvas->toolImageToDocument(point);
}

QPointF KisToolFreehandPaintingInformationBuilder::imageToView(const QPointF &point)
{
    return m_tool->pixelToView(point);
}

QPointF KisToolFreehandPaintingInformationBuilder::adjustDocumentPoint(const QPointF &point, const QPointF &startPoint)
{
    return m_tool->adjustPosition(point, startPoint);
}

qreal KisToolFreehandPaintingInformationBuilder::calculatePerspective(const QPointF &documentPoint)
{
    return m_tool->calculatePerspective(documentPoint);
}

qreal KisToolFreehandPaintingInformationBuilder::canvasRotation() const
{
    KisCanvasToolServices *canvas = dynamic_cast<KisCanvasToolServices*>(m_tool->canvas());
    KIS_ASSERT_RECOVER_RETURN_VALUE(canvas, 0.0);
    return canvas->toolCanvasRotation();
}

bool KisToolFreehandPaintingInformationBuilder::canvasMirroredX() const
{
    KisCanvasToolServices *canvas = dynamic_cast<KisCanvasToolServices*>(m_tool->canvas());
    KIS_ASSERT_RECOVER_RETURN_VALUE(canvas, false);
    return canvas->toolCanvasMirroredHorizontally();
}

bool KisToolFreehandPaintingInformationBuilder::canvasMirroredY() const
{
    KisCanvasToolServices *canvas = dynamic_cast<KisCanvasToolServices*>(m_tool->canvas());
    KIS_ASSERT_RECOVER_RETURN_VALUE(canvas, false);
    return canvas->toolCanvasMirroredVertically();
}
