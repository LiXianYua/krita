/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SvgTextTypeSettingStrategy.h"
#include "SvgTextCursor.h"
#include "SvgTextChangeTransformsOnRange.h"
#include "SvgTextMergePropertiesRangeCommand.h"
#include "SvgTextShapeManagerBlocker.h"

#include <KoToolBase.h>
#include <KoCanvasBase.h>
#include "KoSnapGuide.h"
#include <kis_algebra_2d.h>
#include <KoViewConverter.h>

SvgTextTypeSettingStrategy::SvgTextTypeSettingStrategy(KoToolBase *tool, KoSvgTextShape *textShape, SvgTextCursor *textCursor, const PkRectF &regionOfInterest, Pk::KeyboardModifiers modifiers)
    : KoInteractionStrategy(tool)
    , m_shape(textShape)
    , m_dragStart(regionOfInterest.center())
    , m_deltaCalc(true)
    , m_modifiers(modifiers)
    , m_textData(textShape->getMemento())
{
    m_cursorPos = textCursor->getPos();
    m_cursorAnchor = textCursor->getAnchor();
    m_editingType = textCursor->typeSettingHandleAtPos(regionOfInterest);
    m_referenceCursorPos = textCursor->posForTypeSettingHandleAndRect(SvgTextCursor::TypeSettingModeHandle(m_editingType), regionOfInterest);
}

void SvgTextTypeSettingStrategy::handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers)
{
    PkPointF delta = mouseLocation - m_dragStart;
    m_modifiers = modifiers;

    if (m_modifiers & Pk::ShiftModifier) {
        delta = snapToClosestAxis(delta);
        m_dragCurrent = m_dragStart + delta;
        m_currentDelta = delta;
    } else {
        m_dragCurrent =
            tool()->canvas()->snapGuide()->snap(
                mouseLocation, modifiers);
        m_currentDelta = m_dragCurrent - m_dragStart;
    }


    if (m_editingType != int(SvgTextCursor::NoHandle)) {
        SvgTextShapeManagerBlocker blocker(tool()->canvas()->shapeManager());
        // TODO: replace with KoShapeBulkActionLock (recursive locking is not supported right now)
        PkRectF updateRect = m_shape->boundingRect();
        if (m_previousCmd) {
            m_previousCmd->undo();
        }
        m_previousCmd.reset(createCommand());
        if (m_previousCmd) {
            m_previousCmd->redo();
        }
        updateRect |= m_shape->boundingRect();
        blocker.unlock();
        m_shape->updateAbsolute(updateRect);
    }
}

KUndo2Command *SvgTextTypeSettingStrategy::createCommand()
{
    if (m_editingType == int(SvgTextCursor::NoHandle)) return nullptr;
    PkPointF delta = m_currentDelta;

    PkList<KoSvgTextCharacterInfo> originalTf = m_shape->getPositionsAndRotationsForRange(m_cursorPos, m_cursorAnchor);
    if (originalTf.isEmpty()) return nullptr;

    KUndo2Command *cmd = nullptr;
    if (m_editingType == int(SvgTextCursor::StartPos) || m_editingType == int(SvgTextCursor::EndPos)) {
        if (m_shape->textType() != KoSvgTextShape::PreformattedText && m_shape->textType() != KoSvgTextShape::PrePositionedText) return nullptr;
        SvgTextChangeTransformsOnRange::OffsetType type = m_editingType == int(SvgTextCursor::StartPos)? SvgTextChangeTransformsOnRange::OffsetAll: SvgTextChangeTransformsOnRange::ScaleAndRotate;

        cmd = new SvgTextChangeTransformsOnRange(m_shape, m_cursorPos, m_cursorAnchor, delta, type, m_deltaCalc, nullptr);
    } else {
        const PkPointF dragStart = m_shape->documentToShape(m_dragStart);
        const PkPointF dragCurrent = m_shape->documentToShape(m_dragCurrent);
        const int closestPos = m_referenceCursorPos;
        const PkList<KoSvgTextCharacterInfo> infos = m_shape->getPositionsAndRotationsForRange(closestPos, closestPos);

        if (infos.empty()) return cmd;

        const KoSvgTextCharacterInfo info = infos.first();
        PkTransform rotate;
        rotate.rotate(info.rotateDeg);
        const PkTransform tf = PkTransform::fromTranslate(info.finalPos.x(), info.finalPos.y()) * rotate;
        const PkLineF line = tf.map(PkLineF(PkPointF(), info.advance));
        const qreal distNew = kisDistanceToLine(dragCurrent, line);

        KoSvgTextProperties props;
        KoSvgTextProperties oldProps = m_shape->propertiesForPos(closestPos, true);

        /// Used to synchronise the offsets when changing fontsize...
        PkVector<PkPointF> newPositions;
        PkVector<qreal> newRotations;

        if (m_editingType == int(SvgTextCursor::Ascender) || m_editingType == int(SvgTextCursor::Descender)) {
            const qreal distOld = kisDistanceToLine(dragStart, line);
            const qreal scale = pkMax(0.000001, distNew/distOld);
            KoSvgText::CssLengthPercentage length = oldProps.fontSize();
            length.value *= scale;
            props.setFontSize(length);

            if ((m_shape->textType() == KoSvgTextShape::PreformattedText || m_shape->textType() == KoSvgTextShape::PrePositionedText) && m_cursorPos != m_cursorAnchor) {
                // When we change font size on a selection, we need to correct the offset for the scaled advances.
                // Technically, we need to test against the laid out text to get the correct advance, but that's really complicated...
                PkPointF diff;

                for (const KoSvgTextCharacterInfo &originalInfo : originalTf) {
                    PkTransform rotate;
                    rotate.rotate(originalInfo.rotateDeg);
                    const PkPointF newPos = originalInfo.finalPos + diff;
                    const PkTransform oTf = PkTransform::fromTranslate(originalInfo.finalPos.x(), originalInfo.finalPos.y()) * rotate;
                    diff += (oTf.map(originalInfo.advance*scale) - oTf.map(originalInfo.advance));
                    newPositions.append(newPos);
                    newRotations.append(originalInfo.rotateDeg);
                }
            }
        } else if (m_editingType == int(SvgTextCursor::BaselineShift)) {
            KoSvgText::CssLengthPercentage length;

            const PkLineF normal = line.normalVector();
            const PkPointF normalVector = normal.p2() - normal.p1();
            qreal dot = PkPointF::dotProduct(normalVector, dragCurrent - line.p1());
            length.value = dot < 0? -distNew: distNew;
            props.setProperty(KoSvgTextProperties::BaselineShiftValueId, PkVariant::fromValue(length));
            props.setProperty(KoSvgTextProperties::BaselineShiftModeId, PkVariant::fromValue(KoSvgText::ShiftLengthPercentage));
        } else if (m_editingType == int(SvgTextCursor::LineHeightTop) || m_editingType == int(SvgTextCursor::LineHeightBottom)) {
            KoSvgText::LineHeightInfo lineHeight = oldProps.propertyOrDefault(KoSvgTextProperties::LineHeightId).value<KoSvgText::LineHeightInfo>();
            const qreal metricsMultiplier = oldProps.fontSize().value/qreal(info.metrics.fontSize);

            const qreal ascender = metricsMultiplier*info.metrics.ascender;
            const qreal descender = metricsMultiplier*info.metrics.descender;
            qreal lineGap = distNew - fabs(m_editingType == int(SvgTextCursor::LineHeightTop)? ascender: descender);
            lineHeight.length.value = (ascender-descender)+lineGap+lineGap;
            lineHeight.isNormal = false;
            lineHeight.isNumber = false;

            props.setProperty(KoSvgTextProperties::LineHeightId, PkVariant::fromValue(lineHeight));
        }
        if (!props.isEmpty()) {
            int pos = m_cursorPos == m_cursorAnchor? -1: m_cursorPos;
            int anchor = m_cursorPos == m_cursorAnchor? -1: m_cursorAnchor;
            if (!newPositions.isEmpty()) {
                cmd = new KUndo2Command();
                KUndo2Command *cmd2 = new SvgTextMergePropertiesRangeCommand(m_shape, props, pos, anchor, PkSet<KoSvgTextProperties::PropertyId>(), cmd);
                new SvgTextChangeTransformsOnRange(m_shape, m_cursorPos, m_cursorAnchor, newPositions, newRotations, m_deltaCalc, cmd);
                cmd->setText(cmd2->text());
            } else {
                cmd = new SvgTextMergePropertiesRangeCommand(m_shape, props, pos, anchor);
            }
        }
    }
    return cmd;
}

void SvgTextTypeSettingStrategy::cancelInteraction()
{
    tool()->canvas()->snapGuide()->reset();
    PkRectF updateRect = m_shape->boundingRect();
    if (m_previousCmd) {
        m_previousCmd->undo();
    }
    updateRect |= m_shape->boundingRect();
    m_shape->setMemento(m_textData, m_cursorPos, m_cursorAnchor);
    m_shape->updateAbsolute(updateRect| m_shape->boundingRect());
    tool()->repaintDecorations();
}

void SvgTextTypeSettingStrategy::finishInteraction(Pk::KeyboardModifiers modifiers)
{
    m_modifiers = modifiers;
    cancelInteraction();
}
