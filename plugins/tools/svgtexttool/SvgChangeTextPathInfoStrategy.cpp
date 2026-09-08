/*
 * SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */
#include "SvgChangeTextPathInfoStrategy.h"

#include "KoSvgTextPathInfoChangeCommand.h"
#include <KoPathSegment.h>
#include <KoPathShape.h>
#include <KoCanvasBase.h>
#include <KoToolBase.h>
#include <KoViewConverter.h>
#include <qmath.h>
SvgChangeTextPathInfoStrategy::SvgChangeTextPathInfoStrategy(KoToolBase *tool, KoSvgTextShape *shape, const PkPointF &clicked, int textCursorPos)
    :KoInteractionStrategy(tool)
    , m_shape(shape)
    , m_currentMousePos(clicked)
    , m_textCursorPos(textCursorPos)
{
    KoSvgTextNodeIndex index = m_shape->topLevelNodeForPos(m_textCursorPos);
    m_oldInfo = *(index.textPathInfo());

}

void SvgChangeTextPathInfoStrategy::handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers)
{
    Q_UNUSED(modifiers)

    m_currentMousePos = mouseLocation;

    KUndo2Command *cmd = createCommand();
    if (cmd) {
        cmd->redo();
    }
    tool()->repaintDecorations();
}

KUndo2Command *SvgChangeTextPathInfoStrategy::createCommand()
{
    KoSvgTextNodeIndex index = m_shape->topLevelNodeForPos(m_textCursorPos);
    KoShape *shape = index.textPath();
    if (!shape) {
        return nullptr;
    }

    KoPathShape *path = dynamic_cast<KoPathShape*>(shape);
    KoSvgText::TextOnPathInfo info;

    const qreal grab = tool()->canvas()->viewConverter()->viewToDocumentX(grabSensitivity()) * 4;
    PkRectF roi(0, 0, grab, grab);
    roi.moveCenter(m_currentMousePos);
    KoPathSegment segment = path->segmentAtPoint(m_currentMousePos, roi);

    if (!segment.isValid()) {
        return nullptr;
    }

    PkList<KoPathSegment> segments = path->segmentsAt(path->outlineRect().adjusted(-grab, -grab, grab, grab));

    double length = 0;
    Q_FOREACH(KoPathSegment s, segments) {
        if (s == segment) {
            const PkPointF mouseInShape = path->documentToShape(m_currentMousePos);
            const qreal t = segment.nearestPoint(mouseInShape);
            info.startOffset = length + (t * segment.length());

            const PkLineF l = PkLineF(segment.pointAt(t), mouseInShape).unitVector();
            const PkPointF p1 = l.p2() - l.p1();
            const PkPointF tangent = segment.angleVectorAtParam(t);
            const PkPointF normal(-tangent.y(), tangent.x());
            const qreal dot = PkPointF::dotProduct(p1, normal);
            if (dot <= 0) {
                info.side = KoSvgText::TextPathSideRight;
            } else {
                info.side = KoSvgText::TextPathSideLeft;
            }
        }
        length += s.length();
    }
    if (info.side == KoSvgText::TextPathSideRight) {
        info.startOffset = length - info.startOffset;
    }

    KUndo2Command *cmd = new KoSvgTextPathInfoChangeCommand(m_shape, m_textCursorPos, info);
    cmd->setText(kundo2_i18n("Change Text On Path Position"));
    return cmd;
}

void SvgChangeTextPathInfoStrategy::cancelInteraction()
{
    KUndo2Command *cmd = new KoSvgTextPathInfoChangeCommand(m_shape, m_textCursorPos, m_oldInfo);
    if (cmd) {
        cmd->undo();
    }
    tool()->repaintDecorations();
}

void SvgChangeTextPathInfoStrategy::finishInteraction(Pk::KeyboardModifiers modifiers)
{

}
