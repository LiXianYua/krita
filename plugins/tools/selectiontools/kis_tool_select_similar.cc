/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_select_similar.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QVBoxLayout>

#include <ksharedconfig.h>

#include <KoColorSpace.h>
#include <KisCursorOverrideLock.h>

#include "kis_command_utils.h"
#include "kis_image.h"
#include "kis_iterator_ng.h"
#include "kis_selection_tool_helper.h"
#include "krita_utils.h"
#include <KoPointerEvent.h>
#include <kis_cursor.h>
#include <kis_paint_device.h>
#include <kis_pixel_selection.h>
#include <kis_selection_filters.h>
#include <kis_image_animation_interface.h>
#include <kis_default_bounds.h>
#include <kis_fill_painter.h>

KisToolSelectSimilar::KisToolSelectSimilar(KoCanvasBase *canvas)
    : KisToolSelect(canvas,
                    KisCursor::load("tool_similar_selection_cursor.png", 6, 6),
                    i18n("Similar Color Selection"))
    , m_threshold(20)
    , m_opacitySpread(100)
    , m_previousTime(0)
{
}

void KisToolSelectSimilar::activate(const QSet<KoShape*> &shapes)
{
    KisToolSelect::activate(shapes);
    m_configGroup =  KSharedConfig::openConfig()->group(toolId());

    // Was read in createOptionWidget() (now deleted) when the options panel
    // was created; that ran on every tool activation, so these are the
    // effective defaults with no panel too -- same keys, same fallbacks
    // (including the legacy "fuzziness" -> "threshold" migration).
    if (m_configGroup.hasKey("threshold")) {
        m_threshold = m_configGroup.readEntry("threshold", 20);
    } else {
        m_threshold = m_configGroup.readEntry("fuzziness", 20);
    }
    m_opacitySpread = m_configGroup.readEntry("opacitySpread", 100);
}

void KisToolSelectSimilar::deactivate()
{
    m_referencePaintDevice = nullptr;
    m_referenceNodeList = nullptr;
    KisToolSelect::deactivate();
}

void KisToolSelectSimilar::beginPrimaryAction(KoPointerEvent *event)
{
    KisToolSelectBase::beginPrimaryAction(event);
    if (isMovingSelection()) {
        return;
    }

    KisPaintDeviceSP dev;

    if (!currentNode() ||
        !(currentNode()->projection()) ||
        !selectionEditable()) {

        event->ignore();
        return;
    }

    beginSelectInteraction();

    KisCursorOverrideLock cursorLock(KisCursor::waitCursor());

    // Create the stroke
    KisStrokeStrategyUndoCommandBased *strategy =
            new KisStrokeStrategyUndoCommandBased(kundo2_i18n("Select Similar Color"), false, image().data());
    strategy->setSupportsWrapAroundMode(false);
    KisStrokeId strokeId = image()->startStroke(strategy);

    // Construct the reference device
    if (sampleLayersMode() == SampleCurrentLayer) {
        m_referencePaintDevice = currentNode()->projection();
    } else if (sampleLayersMode() == SampleAllLayers) {
        m_referencePaintDevice = currentImage()->projection();
    } else if (sampleLayersMode() == SampleColorLabeledLayers) {
        if (!m_referenceNodeList) {
            m_referencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Similar Colors Selection Tool Reference Result Paint Device");
            m_referenceNodeList.reset(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
        }
        KisPaintDeviceSP newReferencePaintDevice = KisMergeLabeledLayersCommand::createRefPaintDevice(image(), "Similar Colors Selection Tool Reference Result Paint Device");
        KisMergeLabeledLayersCommand::ReferenceNodeInfoListSP newReferenceNodeList(new KisMergeLabeledLayersCommand::ReferenceNodeInfoList);
        const int currentTime = image()->animationInterface()->currentTime();

        image()->addJob(
            strokeId,
            new KisStrokeStrategyUndoCommandBased::Data(
                KUndo2CommandSP(new KisMergeLabeledLayersCommand(
                    image(),
                    m_referenceNodeList,
                    newReferenceNodeList,
                    m_referencePaintDevice,
                    newReferencePaintDevice,
                    colorLabelsSelected(),
                    KisMergeLabeledLayersCommand::GroupSelectionPolicy_SelectIfColorLabeled,
                    m_previousTime != currentTime
                )),
                false,
                KisStrokeJobData::SEQUENTIAL,
                KisStrokeJobData::EXCLUSIVE
            )
        );

        m_referencePaintDevice = newReferencePaintDevice;
        m_referenceNodeList = newReferenceNodeList;
        m_previousTime = currentTime;
    }

    // Get the color of the pixel where the user clicked
    KisPaintDeviceSP sourceDevice = m_referencePaintDevice;
    const QPoint pos = convertToImagePixelCoordFloored(event);
    QSharedPointer<KoColor> referenceColor = QSharedPointer<KoColor>(new KoColor(sourceDevice->colorSpace()));
    // We need to obtain the reference color from the reference paint
    // device, but it is produced in a stroke, so we must get the color
    // after the device is ready. So we get it in the stroke
    image()->addJob(
        strokeId,
        new KisStrokeStrategyUndoCommandBased::Data(
            KUndo2CommandSP(new KisCommandUtils::LambdaCommand(
                [sourceDevice, referenceColor, pos]() -> KUndo2Command*
                {
                    *referenceColor = sourceDevice->pixel(pos);
                    return 0;
                }
            )),
            false,
            KisStrokeJobData::SEQUENTIAL,
            KisStrokeJobData::EXCLUSIVE
        )
    );

    // Get the similar colors selection
    KisFillPainter painter;
    QRect bounds = currentImage()->bounds();
    QSharedPointer<KisProcessingVisitor::ProgressHelper>
        progressHelper(new KisProcessingVisitor::ProgressHelper(currentNode()));
    KisPixelSelectionSP tmpSel = new KisPixelSelection(new KisSelectionDefaultBounds(currentNode()->projection()));

    painter.setFillThreshold(m_threshold);
    painter.setOpacitySpread(m_opacitySpread);
    painter.setAntiAlias(antiAliasSelection());
    painter.setSizemod(growSelection());
    painter.setStopGrowingAtDarkestPixel(this->stopGrowingAtDarkestPixel());
    painter.setFeather(featherSelection());

    QVector<KisStrokeJobData*> jobs =
        painter.createSimilarColorsSelectionJobs(
            tmpSel, referenceColor, sourceDevice,
            bounds, nullptr, progressHelper
        );

    for (KisStrokeJobData *job : jobs) {
        image()->addJob(strokeId, job);
    }

    image()->addJob(
        strokeId,
        new KisStrokeStrategyUndoCommandBased::Data(
            KUndo2CommandSP(new KisCommandUtils::LambdaCommand(
                [tmpSel]() mutable -> KUndo2Command*
                {
                    tmpSel->invalidateOutlineCache();
                    return 0;
                }
            )),
            false,
            KisStrokeJobData::SEQUENTIAL,
            KisStrokeJobData::EXCLUSIVE
        )
    );

    image()->endStroke(strokeId);

    // Apply selection
    KisSelectionToolHelper helper(
        canvas(), currentImage().toStrongRef(), currentNode(),
        kundo2_i18n("Select Similar Color"));
    helper.selectPixelSelection(tmpSel, selectionAction());
}

void KisToolSelectSimilar::endPrimaryAction(KoPointerEvent *event)
{
    if (isMovingSelection()) {
        KisToolSelectBase::endPrimaryAction(event);
        return;
    }

    endSelectInteraction();
}

void KisToolSelectSimilar::slotSetThreshold(int threshold)
{
    m_threshold = threshold;
    m_configGroup.writeEntry("threshold", threshold);
}

void KisToolSelectSimilar::slotSetOpacitySpread(int opacitySpread)
{
    m_opacitySpread = opacitySpread;
    m_configGroup.writeEntry("opacitySpread", opacitySpread);
}

void KisToolSelectSimilar::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(KisCursor::load("tool_similar_selection_cursor_add.png", 6, 6));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(KisCursor::load("tool_similar_selection_cursor_sub.png", 6, 6));
    } else if (selectionAction() == SELECTION_INTERSECT) {
        useCursor(KisCursor::load("tool_similar_selection_cursor_inter.png", 6, 6));
    } else if (selectionAction() == SELECTION_SYMMETRICDIFFERENCE) {
        useCursor(KisCursor::load("tool_similar_selection_cursor_symdiff.png", 6, 6));
    } else {
        KisToolSelect::resetCursorStyle();
    }
}
