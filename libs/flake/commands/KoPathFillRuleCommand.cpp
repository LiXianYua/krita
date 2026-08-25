/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoPathFillRuleCommand.h"
#include "KoPathShape.h"


class Q_DECL_HIDDEN KoPathFillRuleCommand::Private
{
public:
    Private(Qt::FillRule fillRule) : newFillRule(fillRule) {
    }

    PkList<KoPathShape*> shapes;       ///< the shapes to set fill rule for
    PkList<Qt::FillRule> oldFillRules; ///< the old fill rules, one for each shape
    Qt::FillRule newFillRule;         ///< the new fill rule to set
};

KoPathFillRuleCommand::KoPathFillRuleCommand(const PkList<KoPathShape*> &shapes, Qt::FillRule fillRule, KUndo2Command *parent)
        : KUndo2Command(parent)
        , d(new Private(fillRule))
{
    d->shapes = shapes;
    for (KoPathShape *shape : d->shapes)
        d->oldFillRules.append(shape->fillRule());

    setText(kundo2_text("Set fill rule"));
}

KoPathFillRuleCommand::~KoPathFillRuleCommand()
{
    delete d;
}

void KoPathFillRuleCommand::redo()
{
    KUndo2Command::redo();
    for (KoPathShape *shape : d->shapes) {
        shape->setFillRule(d->newFillRule);
        shape->update();
    }
}

void KoPathFillRuleCommand::undo()
{
    KUndo2Command::undo();
    PkList<Qt::FillRule>::iterator ruleIt = d->oldFillRules.begin();
    for (KoPathShape *shape : d->shapes) {
        shape->setFillRule(*ruleIt);
        shape->update();
        ++ruleIt;
    }
}
