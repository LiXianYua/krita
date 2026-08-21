/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2008 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006, 2007 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>
#include "KoPathPointInsertCommand.h"

#include "KoPathPoint.h"
#include <KoPathSegment.h>

class KoPathPointInsertCommandPrivate
{
public:
    KoPathPointInsertCommandPrivate() : deletePoints(true) { }
    ~KoPathPointInsertCommandPrivate() {
        if (deletePoints) {
            qDeleteAll(points);
        }
    }
    PkList<KoPathPointData> pointDataList;
    PkList<KoPathPoint*> points;
    PkList<PkPair<PkPointF, PkPointF> > controlPoints;
    bool deletePoints;
};

KoPathPointInsertCommand::KoPathPointInsertCommand(const PkList<KoPathPointData> &pointDataList, qreal insertPosition, KUndo2Command *parent)
        : KUndo2Command(parent),
        d(new KoPathPointInsertCommandPrivate())
{
    if (insertPosition < 0)
        insertPosition = 0;
    if (insertPosition > 1)
        insertPosition = 1;

    //TODO the list needs to be sorted

    PkList<KoPathPointData>::const_iterator it(pointDataList.begin());
    for (; it != pointDataList.end(); ++it) {
        KoPathShape * pathShape = it->pathShape;

        KoPathSegment segment = pathShape->segmentByIndex(it->pointIndex);

        // should not happen but to be sure
        if (! segment.isValid())
            continue;

        d->pointDataList.append(*it);

        PkPair<KoPathSegment, KoPathSegment> splitSegments = segment.splitAt(insertPosition);

        KoPathPoint * split1 = splitSegments.first.second();
        KoPathPoint * split2 = splitSegments.second.first();
        KoPathPoint * splitPoint = new KoPathPoint(pathShape, split1->point());
        if(split1->activeControlPoint1())
            splitPoint->setControlPoint1(split1->controlPoint1());
        if(split2->activeControlPoint2())
            splitPoint->setControlPoint2(split2->controlPoint2());

        d->points.append(splitPoint);
        PkPointF cp1 = splitSegments.first.first()->controlPoint2();
        PkPointF cp2 = splitSegments.second.second()->controlPoint1();
        d->controlPoints.append(PkPair<PkPointF, PkPointF>(cp1, cp2));
    }
    setText(kundo2_text("Insert points"));
}

KoPathPointInsertCommand::~KoPathPointInsertCommand()
{
    delete d;
}

void KoPathPointInsertCommand::redo()
{
    KUndo2Command::redo();
    for (int i = d->pointDataList.size() - 1; i >= 0; --i) {
        KoPathPointData pointData = d->pointDataList.at(i);
        KoPathShape * pathShape = pointData.pathShape;

        KoPathSegment segment = pathShape->segmentByIndex(pointData.pointIndex);

        ++pointData.pointIndex.second;

        if (segment.first()->activeControlPoint2()) {
            PkPointF controlPoint2 = segment.first()->controlPoint2();
            std::swap(controlPoint2, d->controlPoints[i].first);
            segment.first()->setControlPoint2(controlPoint2);
        }

        if (segment.second()->activeControlPoint1()) {
            PkPointF controlPoint1 = segment.second()->controlPoint1();
            std::swap(controlPoint1, d->controlPoints[i].second);
            segment.second()->setControlPoint1(controlPoint1);
        }

        pathShape->insertPoint(d->points.at(i), pointData.pointIndex);
        pathShape->recommendPointSelectionChange({pointData.pointIndex});
        pathShape->update();
    }
    d->deletePoints = false;
}

void KoPathPointInsertCommand::undo()
{
    KUndo2Command::undo();
    for (int i = 0; i < d->pointDataList.size(); ++i) {
        const KoPathPointData &pdBefore = d->pointDataList.at(i);
        KoPathShape * pathShape = pdBefore.pathShape;
        KoPathPointIndex piAfter = pdBefore.pointIndex;
        ++piAfter.second;

        KoPathPoint * before = pathShape->pointByIndex(pdBefore.pointIndex);

        d->points[i] = pathShape->removePoint(piAfter);

        if (d->points[i]->properties() & KoPathPoint::CloseSubpath) {
            piAfter.second = 0;
        }

        KoPathPoint * after = pathShape->pointByIndex(piAfter);

        if (before->activeControlPoint2()) {
            PkPointF controlPoint2 = before->controlPoint2();
            std::swap(controlPoint2, d->controlPoints[i].first);
            before->setControlPoint2(controlPoint2);
        }

        if (after->activeControlPoint1()) {
            PkPointF controlPoint1 = after->controlPoint1();
            std::swap(controlPoint1, d->controlPoints[i].second);
            after->setControlPoint1(controlPoint1);
        }

        PkList<KoPathPointIndex> segmentPoints;
        segmentPoints << pdBefore.pointIndex;

        KoPathPointIndex nextPoint(pdBefore.pointIndex.first, pdBefore.pointIndex.second + 1);
        if (pathShape->pointByIndex(nextPoint)) {
            segmentPoints << nextPoint;
        }

        pathShape->recommendPointSelectionChange(segmentPoints);
        pathShape->update();
    }
    d->deletePoints = true;
}

PkList<KoPathPoint*> KoPathPointInsertCommand::insertedPoints() const
{
    return d->points;
}
