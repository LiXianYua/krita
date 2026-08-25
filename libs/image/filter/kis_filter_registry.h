/*
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_FILTER_REGISTRY_H_
#define KIS_FILTER_REGISTRY_H_

#include <PkObject.h>
#include <PkString.h>

#include "kis_filter.h"
#include "kis_types.h"
#include "KoGenericRegistry.h"

#include <kritaimage_export.h>

class KisFilterConfiguration;

class KRITAIMAGE_EXPORT KisFilterRegistry : public PkObject, public KoGenericRegistry<KisFilterSP>
{

    Q_OBJECT

public:

    ~KisFilterRegistry() override;

    static KisFilterRegistry* instance();
    void add(KisFilterSP item);
    void add(const PkString &id, KisFilterSP item);

    KisFilterSP fallbackFilter() const;

Q_SIGNALS:

    void filterAdded(PkString id);

private:

    KisFilterRegistry();
    KisFilterRegistry(const KisFilterRegistry&);
    KisFilterRegistry operator=(const KisFilterRegistry&);

};

#endif // KIS_FILTERSPACE_REGISTRY_H_
