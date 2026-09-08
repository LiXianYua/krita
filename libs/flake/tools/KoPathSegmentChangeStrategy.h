/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOPATHSEGMENTCHANGESTRATEGY_H
#define KOPATHSEGMENTCHANGESTRATEGY_H

#include "KoInteractionStrategy.h"
#include "KoPathSegment.h"
#include "KoPathPointData.h"
#include <PkPoint.h>

class KoPathTool;
class KoPathShape;

/**
* @brief Strategy for deforming a segment of a path shape.
*/
class KoPathSegmentChangeStrategy : public KoInteractionStrategy
{
public:
    KoPathSegmentChangeStrategy(KoPathTool *tool, const PkPointF &pos, const KoPathPointData &segment, qreal segmentParam);
    ~KoPathSegmentChangeStrategy() override;
    void handleMouseMove(const PkPointF &mouseLocation, Pk::KeyboardModifiers modifiers) override;
    void finishInteraction(Pk::KeyboardModifiers modifiers) override;
    KUndo2Command *createCommand() override;

private:
    PkPointF m_originalPosition;
    PkPointF m_lastPosition;
    /// the accumulated point move amount
    PkPointF m_move;
    /// pointer to the path tool
    KoPathTool *m_tool;
    KoPathShape *m_path;
    KoPathSegment m_segment;
    qreal m_segmentParam;
    PkPointF m_ctrlPoint1Move;
    PkPointF m_ctrlPoint2Move;
    KoPathPointData m_pointData1;
    KoPathPointData m_pointData2;
    int m_originalSegmentDegree;
};

#endif // KOPATHSEGMENTCHANGESTRATEGY_H
