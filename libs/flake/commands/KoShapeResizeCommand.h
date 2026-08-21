/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSHAPERESIZECOMMAND_H
#define KOSHAPERESIZECOMMAND_H

#include <PkXmlCompat.h>

#include "kritaflake_export.h"
#include "kundo2command.h"
#include "kis_command_utils.h"

#include <pk/container/PkList.h>
#include <pk/geometry/PkPoint.h>
#include <KoFlake.h>

#include <memory>

class KoShape;


class KRITAFLAKE_EXPORT KoShapeResizeCommand : public KisCommandUtils::SkipFirstRedoBase
{
public:
    KoShapeResizeCommand(const PkList<KoShape*> &shapes,
                         qreal scaleX, qreal scaleY,
                         const PkPointF &absoluteStillPoint, bool useGLobalMode,
                         bool usePostScaling, const PkTransform &postScalingCoveringTransform,
                         KUndo2Command *parent = 0);

    ~KoShapeResizeCommand() override;
    void redoImpl() override;
    void undoImpl() override;

    int id() const override;
    bool mergeWith(const KUndo2Command *command) override;

    void replaceResizeAction(qreal scaleX, qreal scaleY,
                             const PkPointF &absoluteStillPoint);

private:
    void redoNoUpdate();
    void undoNoUpdate();

private:
    struct Private;
    std::unique_ptr<Private> const m_d;

};

#endif // KOSHAPERESIZECOMMAND_H
