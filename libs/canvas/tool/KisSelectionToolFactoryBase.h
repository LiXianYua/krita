/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISSELECTIONTOOLFACTORYBASE_H
#define KISSELECTIONTOOLFACTORYBASE_H

#include <PkList.h>
#include "KisToolPaintFactoryBase.h"

#include "kritacanvas_export.h"

class KRITACANVAS_EXPORT KisSelectionToolFactoryBase : public KisToolPaintFactoryBase
{
public:
    explicit KisSelectionToolFactoryBase(const PkString &id);
    ~KisSelectionToolFactoryBase() override;
protected:
    PkList<KisHostActionSpec> createActionsImpl() override;
};

class KRITACANVAS_EXPORT KisToolPolyLineFactoryBase : public KisToolPaintFactoryBase
{
public:
    explicit KisToolPolyLineFactoryBase(const PkString &id);
    ~KisToolPolyLineFactoryBase() override;
protected:
    PkList<KisHostActionSpec> createActionsImpl() override;
};


#endif 
