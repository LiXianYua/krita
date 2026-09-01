/*
 *  SPDX-FileCopyrightText: 2008 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_GENERATOR_REGISTRY_H_
#define KIS_GENERATOR_REGISTRY_H_

#include <PkObject.h>
#include <PkSignalCompat.h>
#include <PkString.h>

#include "kis_generator.h"
#include "kis_types.h"
#include "KoGenericRegistry.h"

#include <kritaimage_export.h>

class PkString;
class KisFilterConfiguration;

/**
 * XXX_DOCS
 */
class KRITAIMAGE_EXPORT KisGeneratorRegistry : public PkObject, public KoGenericRegistry<KisGeneratorSP>
{
public:
    ~KisGeneratorRegistry() override;

    static KisGeneratorRegistry* instance();
    void add(KisGeneratorSP item);
    void add(const PkString &id, KisGeneratorSP item);

signals:

    void generatorAdded(PkString id);

private:

    KisGeneratorRegistry();
    KisGeneratorRegistry(const KisGeneratorRegistry&);
    KisGeneratorRegistry operator=(const KisGeneratorRegistry&);
};

#endif // KIS_GENERATOR_REGISTRY_H_
