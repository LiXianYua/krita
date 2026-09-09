/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef ELLIPSESHAPECONFIGCOMMAND_H
#define ELLIPSESHAPECONFIGCOMMAND_H

#include "EllipseShape.h"
#include <PkGlobal.h>
#include <kundo2command.h>

/// The undo / redo command for configuring an ellipse shape
class EllipseShapeConfigCommand : public KUndo2Command
{
public:
    /**
     * Configures an ellipse shape
     * @param ellipse the ellipse shape to configure
     * @param type the ellipse type
     * @param startAngle the start angle
     * @param endAngle the end angle
     * @param parent the optional parent command
     */
    EllipseShapeConfigCommand(EllipseShape *ellipse, EllipseShape::EllipseType type, double startAngle, double startEndAngle, KUndo2Command *parent = 0);
    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

    int id() const override;
    bool mergeWith(const KUndo2Command *command) override;

private:
    EllipseShape *m_ellipse;
    EllipseShape::EllipseType m_oldType;
    double m_oldStartAngle;
    double m_oldEndAngle;
    EllipseShape::EllipseType m_newType;
    double m_newStartAngle;
    double m_newEndAngle;
};

#endif // ELLIPSESHAPECONFIGCOMMAND_H
