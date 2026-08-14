/*
 *  kis_tool_select_contiguous - part of Krayon^WKrita
 *
 *  SPDX-FileCopyrightText: 1999 Michael Koch <koch@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2012 José Luis Vergara <pentalis@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_select_contiguous.h"
#include <QPainter>
#include <QLayout>
#include <QApplication>
#include <QVBoxLayout>

#include <kis_debug.h>
#include <klocalizedstring.h>
#include <ksharedconfig.h>

#include "KoPointerEvent.h"
#include "KoViewConverter.h"

#include "kis_cursor.h"
#include "kis_image.h"
#include "KisSelectionUtils.h"
#include "kis_layer.h"
#include "kis_paint_device.h"
#include "kis_fill_painter.h"
#include "kis_pixel_selection.h"
#include "kis_selection_tool_helper.h"
#include "tiles3/kis_hline_iterator.h"
#include "kis_image.h"
#include "kis_undo_stores.h"
#include "kis_resources_snapshot.h"
#include "kis_processing_applicator.h"
#include <processing/fill_processing_visitor.h>
#include <kis_image_animation_interface.h>
#include <KisCursorOverrideLock.h>

#include "kis_command_utils.h"

KisToolSelectContiguous::KisToolSelectContiguous(KoCanvasBase *canvas)
    : KisToolSelect(
        canvas,
        KisCursor::load("tool_contiguous_selection_cursor.png", 6, 6),
        i18n("Contiguous Area Selection"))
    , m_threshold(8)
    , m_opacitySpread(100)
    , m_useSelectionAsBoundary(false)
    , m_previousTime(0)
{
    setObjectName("tool_select_contiguous");
}

KisToolSelectContiguous::~KisToolSelectContiguous()
{
}

void KisToolSelectContiguous::activate(const QSet<KoShape*> &shapes)
{
    KisToolSelect::activate(shapes);
    m_configGroup =  KSharedConfig::openConfig()->group(toolId());

    // Was read in createOptionWidget() (now deleted) when the options panel
    // was created; that ran on every tool activation, so these are the
    // effective defaults with no panel too -- same keys, same fallbacks
    // (including the legacy "fuzziness" -> "threshold" migration).
    const QString contiguousSelectionModeStr =
        m_configGroup.readEntry<QString>("contiguousSelectionMode", "");
    m_contiguousSelectionMode =
        contiguousSelectionModeStr == "boundaryFill"
        ? BoundaryFill
        : FloodFill;
    m_contiguousSelectionBoundaryColor =
        loadContiguousSelectionBoundaryColorFromConfig();
    if (m_configGroup.hasKey("threshold")) {
        m_threshold = m_configGroup.readEntry("threshold", 8);
    } else {
        m_threshold = m_configGroup.readEntry("fuzziness", 8);
    }
    m_opacitySpread = m_configGroup.readEntry("opacitySpread", 100);
    m_closeGap = m_configGroup.readEntry("closeGapAmount", 0);
    m_useSelectionAsBoundary =
        m_configGroup.readEntry("useSelectionAsBoundary", false);
}

void KisToolSelectContiguous::deactivate()
{
    m_referencePaintDevice = nullptr;
    m_referenceNodeList = nullptr;
    KisToolSelect::deactivate();
}

void KisToolSelectContiguous::beginPrimaryAction(KoPointerEvent *event)
{
    KisToolSelectBase::beginPrimaryAction(event);
    if (isMovingSelection()) {
        return;
    }

    KisPaintDeviceSP dev;

    if (!currentNode() ||
        !(dev = currentNode()->projection()) ||
        !selectionEditable()) {
        event->ignore();
        return;
    }

    beginSelectInteraction();

    KisCursorOverrideLock cursorLock(KisCursor::waitCursor());

    // -------------------------------

    KisProcessingApplicator applicator(currentImage(), currentNode(),
                                       KisProcessingApplicator::NONE,
                                       KisImageSignalVector(),
                                       kundo2_i18n("Select Contiguous Area"));

    QPoint pos = convertToImagePixelCoordFloored(event);
    QRect rc = currentImage()->bounds();

    KisPaintDeviceSP sourceDevice;

    if (sampleLayersMode() == SampleCurrentLayer) {
        sourceDevice = m_referencePaintDevice = dev;
    } else if (sampleLayersMode() == SampleAllLayers) {
        sourceDevice = m_referencePaintDevice = currentImage()->projection();
    } else if (sampleLayersMode() == SampleColorLabeledLayers) {
        if (!m_referenceNodeList) {
            m_referencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Contiguous Selection Tool Reference Result Paint Device");
            m_referenceNodeList.reset(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
        }
        KisPaintDeviceSP newReferencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Contiguous Selection Tool Reference Result Paint Device");
        KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP newReferenceNodeList(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
        const int currentTime = image()->animationInterface()->currentTime();
        applicator.applyCommand(
            new KisMergeLabeledLayersCommand(
                image(),
                m_referenceNodeList,
                newReferenceNodeList,
                m_referencePaintDevice,
                newReferencePaintDevice,
                colorLabelsSelected(),
                KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled,
                m_previousTime != currentTime
            ),
            KisStrokeJobData::SEQUENTIAL,
            KisStrokeJobData::EXCLUSIVE
        );
        sourceDevice = m_referencePaintDevice = newReferencePaintDevice;
        m_referenceNodeList = newReferenceNodeList;
        m_previousTime = currentTime;
    }

    if (sampleLayersMode() != SampleColorLabeledLayers) {
        // Reset this so that the device from color labeled layers gets
        // regenerated when that mode is selected again
        m_referenceNodeList.reset();
    }

    KisPixelSelectionSP selection =
        new KisPixelSelection(new KisSelectionDefaultBounds(dev));

    ContiguousSelectionMode contiguousSelectionMode = m_contiguousSelectionMode;
    KoColor contiguousSelectionBoundaryColor = m_contiguousSelectionBoundaryColor;
    int threshold = m_threshold;
    int opacitySpread = m_opacitySpread;
    int closeGap = m_closeGap;
    bool useSelectionAsBoundary = m_useSelectionAsBoundary;
    bool antiAlias = antiAliasSelection();
    int grow = growSelection();
    bool stopGrowingAtDarkestPixel = this->stopGrowingAtDarkestPixel();
    int feather = featherSelection();

    KisPixelSelectionSP existingSelection;
    KisSelectionSP activeSelection = KisSelectionUtils::activeSelectionForNode(
        currentImage().toStrongRef(), currentNode());
    if (activeSelection) {
        existingSelection = activeSelection->pixelSelection();
    }

    KUndo2Command
        *cmd =
            new KisCommandUtils::LambdaCommand(
                [dev,
                 rc,
                 contiguousSelectionMode,
                 contiguousSelectionBoundaryColor,
                 threshold,
                 opacitySpread,
                 closeGap,
                 antiAlias,
                 feather,
                 grow,
                 stopGrowingAtDarkestPixel,
                 useSelectionAsBoundary,
                 selection,
                 pos,
                 sourceDevice,
                 existingSelection]() mutable -> KUndo2Command * {
                    KisFillPainter fillpainter(dev);
                    fillpainter.setHeight(rc.height());
                    fillpainter.setWidth(rc.width());
                    fillpainter.setRegionFillingMode(
                        contiguousSelectionMode == FloodFill
                        ? KisFillPainter::RegionFillingMode_FloodFill
                        : KisFillPainter::RegionFillingMode_BoundaryFill
                    );
                    if (contiguousSelectionMode == BoundaryFill) {
                        fillpainter.setRegionFillingBoundaryColor(contiguousSelectionBoundaryColor);
                    }
                    fillpainter.setFillThreshold(threshold);
                    fillpainter.setOpacitySpread(opacitySpread);
                    fillpainter.setAntiAlias(antiAlias);
                    fillpainter.setFeather(feather);
                    fillpainter.setCloseGap(closeGap);
                    fillpainter.setSizemod(grow);
                    fillpainter.setStopGrowingAtDarkestPixel(stopGrowingAtDarkestPixel);
                    fillpainter.setUseCompositing(true);

                    useSelectionAsBoundary &=
                        existingSelection &&
                        !existingSelection->isEmpty() &&
                        existingSelection->pixel(pos).opacityU8() != OPACITY_TRANSPARENT_U8;

                    fillpainter.setUseSelectionAsBoundary(useSelectionAsBoundary);
                    fillpainter.createFloodSelection(selection, pos.x(), pos.y(), sourceDevice, existingSelection);

                    selection->invalidateOutlineCache();

                    return 0;
                });
    applicator.applyCommand(cmd, KisStrokeJobData::BARRIER);



    KisSelectionToolHelper helper(
        canvas(), currentImage().toStrongRef(), currentNode(),
        kundo2_i18n("Select Contiguous Area"));

    helper.selectPixelSelection(applicator, selection, selectionAction());

    applicator.end();

}

void KisToolSelectContiguous::endPrimaryAction(KoPointerEvent *event)
{
    if (isMovingSelection()) {
        KisToolSelectBase::endPrimaryAction(event);
        return;
    }

    endSelectInteraction();
}

void KisToolSelectContiguous::paint(QPainter &painter, const KoViewConverter &converter)
{
    Q_UNUSED(painter);
    Q_UNUSED(converter);
}

void KisToolSelectContiguous::slotSetContiguousSelectionMode(
    ContiguousSelectionMode contiguousSelectionMode)
{
    if (contiguousSelectionMode == m_contiguousSelectionMode) {
        return;
    }
    m_contiguousSelectionMode = contiguousSelectionMode;
    m_configGroup.writeEntry(
        "contiguousSelectionMode",
        contiguousSelectionMode == FloodFill
        ? "floodFill"
        : "boundaryFill"
    );
}

void KisToolSelectContiguous::slotSetContiguousSelectionBoundaryColor(
    const KoColor &color)
{
    if (color == m_contiguousSelectionBoundaryColor) {
        return;
    }
    m_contiguousSelectionBoundaryColor = color;
    m_configGroup.writeEntry("contiguousSelectionBoundaryColor", color.toXML());
}

void KisToolSelectContiguous::slotSetThreshold(int threshold)
{
    m_threshold = threshold;
    m_configGroup.writeEntry("threshold", threshold);
}

void KisToolSelectContiguous::slotSetOpacitySpread(int opacitySpread)
{
    m_opacitySpread = opacitySpread;
    m_configGroup.writeEntry("opacitySpread", opacitySpread);
}

void KisToolSelectContiguous::slotSetCloseGap(int closeGap)
{
    m_closeGap = closeGap;
    m_configGroup.writeEntry("closeGapAmount", closeGap);
}

void KisToolSelectContiguous::slotSetUseSelectionAsBoundary(bool useSelectionAsBoundary)
{
    m_useSelectionAsBoundary = useSelectionAsBoundary;
    m_configGroup.writeEntry("useSelectionAsBoundary", useSelectionAsBoundary);
}


KoColor KisToolSelectContiguous::loadContiguousSelectionBoundaryColorFromConfig()
{
    const QString xmlColor =
        m_configGroup.readEntry("contiguousSelectionBoundaryColor", QString());
    QDomDocument doc;
    if (doc.setContent(xmlColor)) {
        QDomElement e = doc.documentElement().firstChild().toElement();
        QString channelDepthID =
            doc.documentElement().attribute("channeldepth",
                                            Integer16BitsColorDepthID.id());
        bool ok;
        if (e.hasAttribute("space") || e.tagName().toLower() == "srgb") {
            return KoColor::fromXML(e, channelDepthID, &ok);
        } else if (doc.documentElement().hasAttribute("space") ||
                   doc.documentElement().tagName().toLower() == "srgb") {
            return KoColor::fromXML(doc.documentElement(), channelDepthID, &ok);
        }
    }
    return KoColor();
}

void KisToolSelectContiguous::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(KisCursor::load("tool_contiguous_selection_cursor_add.png", 6, 6));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(KisCursor::load("tool_contiguous_selection_cursor_sub.png", 6, 6));
    } else if (selectionAction() == SELECTION_INTERSECT) {
        useCursor(KisCursor::load("tool_contiguous_selection_cursor_inter.png", 6, 6));
    } else if (selectionAction() == SELECTION_SYMMETRICDIFFERENCE) {
        useCursor(KisCursor::load("tool_contiguous_selection_cursor_symdiff.png", 6, 6));
    } else {
        KisToolSelect::resetCursorStyle();
    }
}
