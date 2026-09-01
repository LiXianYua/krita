/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SHAPERESIZESTRATEGY_H
#define SHAPERESIZESTRATEGY_H

#include <KoInteractionStrategy.h>
#include <KoFlake.h>

#include <PkScopedPointer.h>
#include <PkPoint.h>
#include <PkList.h>
#include <PkTransform.h>

#include <memory>

class KoToolBase;
class KoShape;
class KoShapeResizeCommand;
class KoSelection;

/**
 * A strategy for the KoInteractionTool.
 * This strategy is invoked when the user starts a resize of a selection of objects,
 * the strategy will then resize the objects interactively and provide a command afterwards.
 */
class ShapeResizeStrategy : public KoInteractionStrategy
{
public:
    /**
     * Constructor
     */
    ShapeResizeStrategy(KoToolBase *tool, KoSelection *selection, const PkPointF &clicked, KoFlake::SelectionHandle direction, bool forceUniformScalingMode);
    ~ShapeResizeStrategy() override;

    void handleMouseMove(const PkPointF &mouseLocation, Qt::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;
    void finishInteraction(Qt::KeyboardModifiers modifiers) override;
    void paint(PkPainter &painter, const KoViewConverter &converter) override;
private:
    void resizeBy(const PkPointF &stillPoint, qreal zoomX, qreal zoomY);

    PkPointF m_start;
    PkList<KoShape *> m_selectedShapes;

    PkTransform m_postScalingCoveringTransform;
    PkSizeF m_initialSelectionSize;
    PkTransform m_unwindMatrix;
    bool m_top {false};
    bool m_left{false};
    bool m_bottom {false};
    bool m_right {false};

    PkPointF m_globalStillPoint;
    PkPointF m_globalCenterPoint;
    std::unique_ptr<KoShapeResizeCommand> m_executedCommand;

    bool m_forceUniformScalingMode {false};
};

#endif

