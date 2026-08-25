/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2009 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006, 2007 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOPATHSEGMENTTYPECOMMAND_H
#define KOPATHSEGMENTTYPECOMMAND_H
#include <PkList.h>
#include <PkPoint.h>


#include <kundo2command.h>
#include "KoPathPoint.h"
#include "KoPathPointData.h"
#include "kritaflake_export.h"

/// The undo / redo command for changing segments to curves/lines
class KRITAFLAKE_EXPORT KoPathSegmentTypeCommand : public KUndo2Command
{
public:
    /// Segment Types
    enum SegmentType {
        Curve = 1,
        Line = 2
    };

    /**
    * Command for changing the segment type ( curve/line )
    * @param pointData point data identifying the segment that should be changed.
    * @param segmentType to which the segment should be changed to
    * @param parent the parent command used for macro commands
    */
    KoPathSegmentTypeCommand(const KoPathPointData &pointData, SegmentType segmentType, KUndo2Command *parent = 0);

    /**
     * Command for changing the segment type ( curve/line )
     * @param pointDataList List of point data identifying the segments that should be changed.
     * @param segmentType to which the segments should be changed to
     * @param parent the parent command used for macro commands
     */
    KoPathSegmentTypeCommand(const PkList<KoPathPointData> &pointDataList, SegmentType segmentType, KUndo2Command *parent = 0);
    ~KoPathSegmentTypeCommand() override;

    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

private:
    // used for storing the data for undo
    struct SegmentTypeData {
        // old control points in document coordinates
        PkPointF m_controlPoint1;
        PkPointF m_controlPoint2;
        KoPathPoint::PointProperties m_properties1;
        KoPathPoint::PointProperties m_properties2;
    };

    void initialize(const PkList<KoPathPointData> &pointDataList);

    PkList<KoPathPointData> m_pointDataList;
    PkList<SegmentTypeData> m_segmentData;
    SegmentType m_segmentType;
};

#endif // KOPATHSEGMENTTYPECOMMAND_H
