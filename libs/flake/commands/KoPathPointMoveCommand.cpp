/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2008-2009 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006, 2007 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include <pk/container/PkMap.h>
#include <pk/container/PkSet.h>
#include "KoPathPointMoveCommand.h"
#include "KoPathPoint.h"
#include "kis_command_ids.h"
#include "krita_container_utils.h"
#include <KoShapeBulkActionLock.h>

class KoPathPointMoveCommandPrivate
{
public:
    KoPathPointMoveCommandPrivate() { }
    void applyOffset(qreal factor);

    PkMap<KoPathPointData, PkPointF > points;
    PkSet<KoPathShape*> paths;
};


KoPathPointMoveCommand::KoPathPointMoveCommand(const PkList<KoPathPointData> &pointData, const PkPointF &offset, KUndo2Command *parent)
    : KUndo2Command(parent),
    d(new KoPathPointMoveCommandPrivate())
{
    setText(kundo2_text("Move points"));

    for (const KoPathPointData &data : pointData) {
        if (!d->points.contains(data)) {
            d->points[data] = offset;
            d->paths.insert(data.pathShape);
        }
    }
}

KoPathPointMoveCommand::KoPathPointMoveCommand(const PkList<KoPathPointData> &pointData, const PkList<PkPointF> &offsets, KUndo2Command *parent)
    : KUndo2Command(parent),
    d(new KoPathPointMoveCommandPrivate())
{
    Q_ASSERT(pointData.count() == offsets.count());

    setText(kundo2_text("Move points"));

    uint dataCount = pointData.count();
    for (uint i = 0; i < dataCount; ++i) {
        const KoPathPointData & data = pointData[i];
        if (!d->points.contains(data)) {
            d->points[data] = offsets[i];
            d->paths.insert(data.pathShape);
        }
    }
}

KoPathPointMoveCommand::~KoPathPointMoveCommand()
{
    delete d;
}

void KoPathPointMoveCommand::redo()
{
    KUndo2Command::redo();
    d->applyOffset(1.0);
}

void KoPathPointMoveCommand::undo()
{
    KUndo2Command::undo();
    d->applyOffset(-1.0);
}

int KoPathPointMoveCommand::id() const
{
    return KisCommandUtils::ChangePathShapePointId;
}

bool KoPathPointMoveCommand::mergeWith(const KUndo2Command *command)
{
    const KoPathPointMoveCommand *other = dynamic_cast<const KoPathPointMoveCommand*>(command);

    if (!other ||
        other->d->paths != d->paths ||
        !KritaUtils::compareListsUnordered(other->d->points.keys(), d->points.keys())) {

        return false;
    }

    auto it = d->points.begin();
    while (it != d->points.end()) {
        it.value() += other->d->points[it.key()];
        ++it;
    }

    return true;
}

void KoPathPointMoveCommandPrivate::applyOffset(qreal factor)
{
    PkList<KoShape*> shapes;
    std::copy(paths.begin(), paths.end(), std::back_inserter(shapes));

    KoShapeBulkActionLock lock(toQList(shapes));

    PkMap<KoPathPointData, PkPointF>::iterator it(points.begin());
    for (; it != points.end(); ++it) {
        KoPathShape *path = it.key().pathShape;
        // transform offset from document to shape coordinate system
        QPointF shapeOffset = path->documentToShape(toQPointF(factor*it.value())) - path->documentToShape(toQPointF(PkPointF()));
        PkTransform matrix;
        matrix.translate(shapeOffset.x(), shapeOffset.y());

        KoPathPoint *p = path->pointByIndex(it.key().pointIndex);
        if (p)
            p->map(toQTransform(matrix));
    }

    for (KoPathShape *path : paths) {
        path->normalize();
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}
