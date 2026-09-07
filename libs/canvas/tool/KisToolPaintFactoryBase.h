/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISTOOLPAINTFACTORYBASE_H
#define KISTOOLPAINTFACTORYBASE_H

#include <PkList.h>
#include <KoToolFactoryBase.h>

#include "kritacanvas_export.h"

class KRITACANVAS_EXPORT KisToolPaintFactoryBase : public KoToolFactoryBase
{
public:
    explicit KisToolPaintFactoryBase(const PkString &id);
    ~KisToolPaintFactoryBase() override;
protected:
    PkList<QAction *> createActionsImpl() override;

};

#endif // KISTOOLPAINTFACTORYBASE_H
