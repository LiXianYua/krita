/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISSELECTIONTOOLFACTORYBASE_H
#define KISSELECTIONTOOLFACTORYBASE_H

#include "KisToolPaintFactoryBase.h"

#include "kritacanvas_export.h"

class KRITACANVAS_EXPORT KisSelectionToolFactoryBase : public KisToolPaintFactoryBase
{
public:
    explicit KisSelectionToolFactoryBase(const QString &id);
    ~KisSelectionToolFactoryBase() override;
protected:
    QList<QAction *> createActionsImpl() override;
};

class KRITACANVAS_EXPORT KisToolPolyLineFactoryBase : public KisToolPaintFactoryBase
{
public:
    explicit KisToolPolyLineFactoryBase(const QString &id);
    ~KisToolPolyLineFactoryBase() override;
protected:
    QList<QAction *> createActionsImpl() override;
};


#endif 
