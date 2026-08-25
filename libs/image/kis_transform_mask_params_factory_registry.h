/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TRANSFORM_MASK_PARAMS_FACTORY_REGISTRY_H
#define __KIS_TRANSFORM_MASK_PARAMS_FACTORY_REGISTRY_H

#include <PkMap.h>
#include <functional>

#include "kis_types.h"
#include "kritaimage_export.h"

#include "kis_transform_mask_params_interface.h"


class PkXmlElement;

using KisTransformMaskParamsFactory    = std::function<KisTransformMaskParamsInterfaceSP (const PkXmlElement &)>;
using KisTransformMaskParamsFactoryMap = PkMap<PkString, KisTransformMaskParamsFactory>;
using KisAnimatedTransformMaskParamsHolderFactory = std::function<KisAnimatedTransformParamsHolderInterfaceSP (KisDefaultBoundsBaseSP)>;

class KRITAIMAGE_EXPORT KisTransformMaskParamsFactoryRegistry
{

public:
    KisTransformMaskParamsFactoryRegistry();
    ~KisTransformMaskParamsFactoryRegistry();

    void addFactory(const PkString &id, const KisTransformMaskParamsFactory &factory);
    KisTransformMaskParamsInterfaceSP createParams(const PkString &id, const PkXmlElement &e);

    void setAnimatedParamsHolderFactory(const KisAnimatedTransformMaskParamsHolderFactory &factory);
    KisAnimatedTransformParamsHolderInterfaceSP createAnimatedParamsHolder(KisDefaultBoundsBaseSP defaultBounds);

    static KisTransformMaskParamsFactoryRegistry* instance();

private:
    KisTransformMaskParamsFactoryMap m_map;
    KisAnimatedTransformMaskParamsHolderFactory m_animatedParamsFactory;
};

#endif /* __KIS_TRANSFORM_MASK_PARAMS_FACTORY_REGISTRY_H */
