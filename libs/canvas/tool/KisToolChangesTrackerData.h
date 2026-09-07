/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISTOOLCHANGESTRACKERDATA_H
#define KISTOOLCHANGESTRACKERDATA_H

#include <kritacanvas_export.h>
#include <PkSharedPointer.h>

class KRITACANVAS_EXPORT KisToolChangesTrackerData
{
public:
    virtual ~KisToolChangesTrackerData();
    virtual KisToolChangesTrackerData* clone() const;
};

typedef PkSharedPointer<KisToolChangesTrackerData> KisToolChangesTrackerDataSP;

#endif // KISTOOLCHANGESTRACKERDATA_H
